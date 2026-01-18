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
 * @file benchmark_common.cpp
 * @brief Implementation of benchmark helper functions
 */

#include <filesystem>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "benchmark_common.hpp"

namespace common_benchmark_helpers
{
    std::string getExecutablePathAndName()
    {
        std::vector<char> buffer(2048);
        ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (len != -1)
        {
            buffer[len] = '\0';
            return std::string(buffer.data());
        }
        return {};
    }

    std::string getOnlyExecutablePath()
    {
        std::filesystem::path p(getExecutablePathAndName());
        return p.parent_path().string() + "/"; // Return only the directory
    }

    // Helper function to get the path to the benchmark_db_connections.yml file
    std::string getConfigFilePath()
    {
        // The YAML file is copied to the build directory by CMake
        return getOnlyExecutablePath() + "benchmark_db_connections.yml";
    }

    // Helper function to generate random string data
    std::string generateRandomString(size_t length)
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

    // Helper function to generate random IDs
    std::vector<int> generateRandomIds(int maxId, int count)
    {
        // Initialize vector and random number generator
        std::vector<int> result;
        result.reserve(count);

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, maxId);

        // Generate unique random IDs
        std::unordered_set<int> usedIds;
        while (usedIds.size() < static_cast<size_t>(count) && usedIds.size() < static_cast<size_t>(maxId))
        {
            int id = dist(gen);
            if (usedIds.find(id) == usedIds.end())
            {
                usedIds.insert(id);
                result.push_back(id);
            }
        }

        return result;
    }

    // Implementation of createBenchmarkTable function
    void createBenchmarkTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName)
    {
        // Drop table if it exists
        try
        {
            conn->executeUpdate("DROP TABLE IF EXISTS " + tableName);
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampError("Error dropping table: " + std::string(e.what()));
        }

        // Check if this is a PostgreSQL connection
        // Check if this is a PostgreSQL connection based on table name
        bool isPostgreSQL = false;
        if (conn->getURL().find("postgresql") != std::string::npos)
        {
            isPostgreSQL = true;
        }

        // Create table with standard columns for benchmarks
        try
        {
            // Use DOUBLE PRECISION for PostgreSQL, DOUBLE for other databases
            std::string floatType = isPostgreSQL ? "DOUBLE PRECISION" : "DOUBLE";

            conn->executeUpdate(
                "CREATE TABLE " + tableName + " ("
                                              "id INT PRIMARY KEY, "
                                              "name VARCHAR(100), "
                                              "value " +
                floatType + ", "
                            "description TEXT, "
                            "created_at TIMESTAMP"
                            ")");
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            throw; // Re-throw to fail the test
        }
    }

    // Implementation of dropBenchmarkTable function
    void dropBenchmarkTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName)
    {
        try
        {
            conn->executeUpdate("DROP TABLE IF EXISTS " + tableName);
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampError("Error dropping table: " + std::string(e.what()));
        }
    }

    // Implementation of populateTable function
    void populateTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName, int rowCount)
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
}

namespace mysql_benchmark_helpers
{

#if USE_MYSQL
    // Track which tables have been initialized to avoid recreating them
    static std::unordered_map<std::string, bool> tableInitialized;
    static std::mutex tableMutex;

    cpp_dbc::config::DatabaseConfig getMySQLConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_benchmark_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("mysql");
            dbConfig.setHost("localhost");
            dbConfig.setPort(3306);
            dbConfig.setDatabase("Test01DB");
            dbConfig.setUsername("root");
            dbConfig.setPassword("dsystems");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("mysql");
        dbConfig.setHost("localhost");
        dbConfig.setPort(3306);
        dbConfig.setDatabase("Test01DB");
        dbConfig.setUsername("root");
        dbConfig.setPassword("dsystems");
#endif

        return dbConfig;
    }

    bool canConnectToMySQL()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getMySQLConfig("dev_mysql");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the MySQL driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

            // Attempt to connect to MySQL
            cpp_dbc::system_utils::logWithTimestampInfo("Attempting to connect to MySQL with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimestampInfo("MySQL connection successful!");

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value");
            bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return false;
        }
    }

    // Implementation of setupMySQLConnection
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupMySQLConnection(const std::string &tableName, int rowCount)
    {
        try
        {
            // Get database configuration
            auto dbConfig = getMySQLConfig("dev_mysql");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Check if this table has already been initialized
            bool needsInitialization = false;
            {
                std::lock_guard<std::mutex> lock(tableMutex);
                auto it = tableInitialized.find(tableName);
                if (it == tableInitialized.end() || !it->second)
                {
                    needsInitialization = true;
                    tableInitialized[tableName] = true;
                }
            }

            // Register the MySQL driver and get a connection
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (needsInitialization)
            {
                // Create benchmark table
                cpp_dbc::system_utils::logWithTimestampInfo("Creating and populating table '" + tableName + "' for the first time...");
                common_benchmark_helpers::createBenchmarkTable(conn, tableName);

                // Populate table with specified rows if rowCount > 0
                if (rowCount > 0)
                {
                    common_benchmark_helpers::populateTable(conn, tableName, rowCount);
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimestampInfo("Reusing existing table '" + tableName + "'");
            }

            return conn;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return nullptr;
        }
    }

#endif // USE_MYSQL

} // namespace mysql_benchmark_helpers

namespace postgresql_benchmark_helpers
{

#if USE_POSTGRESQL
    // Track which tables have been initialized to avoid recreating them
    static std::unordered_map<std::string, bool> tableInitialized;
    static std::mutex tableMutex;

    cpp_dbc::config::DatabaseConfig getPostgreSQLConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_benchmark_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("postgresql");
            dbConfig.setHost("localhost");
            dbConfig.setPort(5432);
            dbConfig.setDatabase("Test01DB");
            dbConfig.setUsername("root");
            dbConfig.setPassword("dsystems");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("postgresql");
        dbConfig.setHost("localhost");
        dbConfig.setPort(5432);
        dbConfig.setDatabase("Test01DB");
        dbConfig.setUsername("root");
        dbConfig.setPassword("dsystems");
#endif

        return dbConfig;
    }

    bool canConnectToPostgreSQL()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getPostgreSQLConfig("dev_postgresql");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the PostgreSQL driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

            // Attempt to connect to PostgreSQL
            cpp_dbc::system_utils::logWithTimestampInfo("Attempting to connect to PostgreSQL with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimestampInfo("PostgreSQL connection successful!");

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value");
            bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return false;
        }
    }

    // Implementation of setupPostgreSQLConnection
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupPostgreSQLConnection(const std::string &tableName, int rowCount)
    {
        try
        {
            // Get database configuration
            auto dbConfig = getPostgreSQLConfig("dev_postgresql");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Check if this table has already been initialized
            bool needsInitialization = false;
            {
                std::lock_guard<std::mutex> lock(tableMutex);
                auto it = tableInitialized.find(tableName);
                if (it == tableInitialized.end() || !it->second)
                {
                    needsInitialization = true;
                    tableInitialized[tableName] = true;
                }
            }

            // Register the PostgreSQL driver and get a connection
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (needsInitialization)
            {
                // Create benchmark table
                cpp_dbc::system_utils::logWithTimestampInfo("Creating and populating table '" + tableName + "' for the first time...");
                common_benchmark_helpers::createBenchmarkTable(conn, tableName);

                // Populate table with specified rows if rowCount > 0
                if (rowCount > 0)
                {
                    common_benchmark_helpers::populateTable(conn, tableName, rowCount);
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimestampInfo("Reusing existing table '" + tableName + "'");
            }

            return conn;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return nullptr;
        }
    }

#endif // USE_POSTGRESQL

} // namespace postgresql_benchmark_helpers

namespace sqlite_benchmark_helpers
{

#if USE_SQLITE
    // Track which tables have been initialized to avoid recreating them
    static std::unordered_map<std::string, bool> tableInitialized;
    static std::mutex tableMutex;

    cpp_dbc::config::DatabaseConfig getSQLiteConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_benchmark_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("sqlite");
            // Create a temporary file with a random name
            std::string tempDbPath = "/tmp/benchmark_sqlite_" +
                                     std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                                     "_" + common_benchmark_helpers::generateRandomString(8) + ".db";
            dbConfig.setDatabase(tempDbPath);
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("sqlite");
        // Create a temporary file with a random name
        std::string tempDbPath = "/tmp/benchmark_sqlite_" +
                                 std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                                 "_" + common_benchmark_helpers::generateRandomString(8) + ".db";
        dbConfig.setDatabase(tempDbPath);
#endif

        return dbConfig;
    }

    std::string getSQLiteConnectionString()
    {
        auto dbConfig = getSQLiteConfig("dev_sqlite");

        // std::string connStr = "cpp_dbc:" + type + "://" + database;

        std::string connStr = "cpp_dbc:" + dbConfig.getType() + "://" + dbConfig.getDatabase();
        return connStr;
        // return dbConfig.createConnectionString();
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
            cpp_dbc::system_utils::logWithTimestampInfo("Attempting to connect to SQLite with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimestampInfo("SQLite connection successful!");

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value");
            bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return false;
        }
    }

    // Helper function to setup SQLite connection
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupSQLiteConnection(const std::string &tableName, int rowCount)
    {
        try
        {
            // Get connection string using the centralized helper
            std::string connStr = getSQLiteConnectionString();

            // Check if this table has already been initialized
            bool needsInitialization = false;
            {
                std::lock_guard<std::mutex> lock(tableMutex);
                auto it = tableInitialized.find(tableName);
                if (it == tableInitialized.end() || !it->second)
                {
                    // Get database configuration
                    auto dbConfig = getSQLiteConfig("dev_sqlite");

                    // Get the database file path
                    std::string dbPath = dbConfig.getDatabase();

                    // Delete the database file if it exists to ensure a clean state
                    if (std::filesystem::exists(dbPath))
                    {
                        cpp_dbc::system_utils::logWithTimestampInfo("Removing existing SQLite database file: " + dbPath);
                        std::filesystem::remove(dbPath);
                    }

                    needsInitialization = true;
                    tableInitialized[tableName] = true;
                }
            }

            // Register the SQLite driver and get a connection
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

            if (needsInitialization)
            {
                // Create benchmark table
                cpp_dbc::system_utils::logWithTimestampInfo("Creating and populating table '" + tableName + "' for the first time...");
                common_benchmark_helpers::createBenchmarkTable(conn, tableName);

                // Populate table with specified rows if rowCount > 0
                if (rowCount > 0)
                {
                    common_benchmark_helpers::populateTable(conn, tableName, rowCount);
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimestampInfo("Reusing existing table '" + tableName + "'");
            }

            return conn;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return nullptr;
        }
    }

#endif // USE_SQLITE

} // namespace sqlite_benchmark_helpers

namespace firebird_benchmark_helpers
{

#if USE_FIREBIRD
    // Track which tables have been initialized to avoid recreating them
    static std::unordered_map<std::string, bool> tableInitialized;
    static std::mutex tableMutex;

    cpp_dbc::config::DatabaseConfig getFirebirdConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_benchmark_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("firebird");
            dbConfig.setHost("localhost");
            dbConfig.setPort(3050);
            dbConfig.setDatabase("/var/lib/firebird/data/test.fdb");
            dbConfig.setUsername("SYSDBA");
            dbConfig.setPassword("masterkey");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("firebird");
        dbConfig.setHost("localhost");
        dbConfig.setPort(3050);
        dbConfig.setDatabase("/var/lib/firebird/data/test.fdb");
        dbConfig.setUsername("SYSDBA");
        dbConfig.setPassword("masterkey");
#endif

        return dbConfig;
    }

    bool canConnectToFirebird()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getFirebirdConfig("dev_firebird");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the Firebird driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

            // Attempt to connect to Firebird
            cpp_dbc::system_utils::logWithTimestampInfo("Attempting to connect to Firebird with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimestampInfo("Firebird connection successful!");

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value FROM RDB$DATABASE");
            bool success = resultSet->next() && resultSet->getInt("TEST_VALUE") == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return false;
        }
    }

    // Helper function to create Firebird-specific benchmark table
    void createFirebirdBenchmarkTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName)
    {
        // Drop table if it exists - Firebird doesn't support IF EXISTS, so we need to handle the error
        try
        {
            conn->executeUpdate("DROP TABLE " + tableName);
            conn->commit();
        }
        catch (const std::exception &e)
        {
            // Table might not exist, which is fine
            try
            {
                conn->rollback();
            }
            catch (...)
            {
                // Ignore rollback errors
            }
        }

        // Create table with standard columns for benchmarks
        // Firebird uses DOUBLE PRECISION for floating point numbers
        try
        {
            conn->executeUpdate(
                "CREATE TABLE " + tableName + " ("
                                              "id INTEGER NOT NULL PRIMARY KEY, "
                                              "name VARCHAR(100), "
                                              "num_value DOUBLE PRECISION, "
                                              "description BLOB SUB_TYPE TEXT, "
                                              "created_at TIMESTAMP"
                                              ")");
            conn->commit();
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            throw; // Re-throw to fail the test
        }
    }

    // Helper function to populate Firebird table with test data
    void populateFirebirdTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName, int rowCount)
    {
        // Prepare the insert statement
        // Firebird uses CURRENT_TIMESTAMP for the current timestamp
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, num_value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");

        // Insert the specified number of rows
        for (int i = 1; i <= rowCount; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            pstmt->executeUpdate();
        }

        // Commit the inserts
        conn->commit();
    }

    // Implementation of setupFirebirdConnection
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupFirebirdConnection(const std::string &tableName, int rowCount)
    {
        try
        {
            // Get database configuration
            auto dbConfig = getFirebirdConfig("dev_firebird");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Check if this table has already been initialized
            bool needsInitialization = false;
            {
                std::lock_guard<std::mutex> lock(tableMutex);
                auto it = tableInitialized.find(tableName);
                if (it == tableInitialized.end() || !it->second)
                {
                    needsInitialization = true;
                    tableInitialized[tableName] = true;
                }
            }

            // Register the Firebird driver and get a connection
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (needsInitialization)
            {
                // Create benchmark table
                cpp_dbc::system_utils::logWithTimestampInfo("Creating and populating table '" + tableName + "' for the first time...");
                createFirebirdBenchmarkTable(conn, tableName);

                // Populate table with specified rows if rowCount > 0
                if (rowCount > 0)
                {
                    populateFirebirdTable(conn, tableName, rowCount);
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimestampInfo("Reusing existing table '" + tableName + "'");
            }

            return conn;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return nullptr;
        }
    }

#endif // USE_FIREBIRD

} // namespace firebird_benchmark_helpers

namespace mongodb_benchmark_helpers
{

#if USE_MONGODB
    // Track which collections have been initialized to avoid recreating them
    static std::unordered_map<std::string, bool> collectionInitialized;
    static std::mutex collectionMutex;

    cpp_dbc::config::DatabaseConfig getMongoDBConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_benchmark_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("mongodb");
            dbConfig.setHost("localhost");
            dbConfig.setPort(27017);
            dbConfig.setDatabase("test01db");
            dbConfig.setUsername("");
            dbConfig.setPassword("");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("mongodb");
        dbConfig.setHost("localhost");
        dbConfig.setPort(27017);
        dbConfig.setDatabase("test01db");
        dbConfig.setUsername("");
        dbConfig.setPassword("");
#endif

        return dbConfig;
    }

    // Build a MongoDB connection string from a DatabaseConfig
    std::string buildMongoDBConnectionString(const cpp_dbc::config::DatabaseConfig &dbConfig)
    {
        // Format: mongodb://[username:password@]host:port/database?options
        // Use the cpp_dbc: prefix for consistency with other database drivers
        std::string connStr = "cpp_dbc:mongodb://";

        if (!dbConfig.getUsername().empty() && !dbConfig.getPassword().empty())
        {
            connStr += dbConfig.getUsername() + ":" + dbConfig.getPassword() + "@";
        }

        connStr += dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort());
        connStr += "/" + dbConfig.getDatabase();

        // Add query parameters from options
        std::string queryParams;

        // Check for auth_source option
        auto authSource = dbConfig.getOption("auth_source");
        if (!authSource.empty())
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "authSource=" + authSource;
        }

        // Check for direct_connection option
        auto directConnection = dbConfig.getOption("direct_connection");
        if (!directConnection.empty() && directConnection == "true")
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "directConnection=true";
        }

        // Check for connect_timeout option
        auto connectTimeout = dbConfig.getOption("connect_timeout");
        if (!connectTimeout.empty())
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "connectTimeoutMS=" + connectTimeout;
        }

        // Check for server_selection_timeout option
        auto serverSelectionTimeout = dbConfig.getOption("server_selection_timeout");
        if (!serverSelectionTimeout.empty())
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "serverSelectionTimeoutMS=" + serverSelectionTimeout;
        }

        // Append query parameters if any
        if (!queryParams.empty())
        {
            connStr += "?" + queryParams;
        }

        return connStr;
    }

    bool canConnectToMongoDB()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getMongoDBConfig("dev_mongodb");

            // Get connection parameters
            std::string connStr = buildMongoDBConnectionString(dbConfig);
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the MongoDB driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());

            // Attempt to connect to MongoDB
            cpp_dbc::system_utils::logWithTimestampInfo("Attempting to connect to MongoDB with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimestampInfo("MongoDB connection successful!");

            // Execute a simple query to verify the connection
            bool success = true;
            try
            {
                auto collection = conn->getCollection("system.version");
                auto cursor = collection->find("{}");
                success = (cursor != nullptr);
            }
            catch (const std::exception &e)
            {
                cpp_dbc::system_utils::logWithTimestampException(e.what());
                success = false;
            }

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return false;
        }
    }

    // Helper function to get a MongoDB driver instance
    std::shared_ptr<cpp_dbc::MongoDB::MongoDBDriver> getMongoDBDriver()
    {
        // Create and return a MongoDB driver
        return std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
    }

    // Helper function to create a test document
    std::string generateTestDocument(int id, const std::string &name, double value, const std::string &description)
    {
        return "{"
               "\"id\": " +
               std::to_string(id) + ", "
                                    "\"name\": \"" +
               name + "\", "
                      "\"value\": " +
               std::to_string(value) + ", "
                                       "\"description\": \"" +
               description + "\", "
                             "\"created_at\": { \"$date\": \"" +
               [&]()
        {
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto timestamp_ms = now_ms.time_since_epoch().count();

            // Format as ISO 8601 date
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::tm *now_tm = std::gmtime(&now_c);

            std::stringstream ss;
            ss << std::put_time(now_tm, "%Y-%m-%dT%H:%M:%S") << "."
               << std::setfill('0') << std::setw(3) << (timestamp_ms % 1000) << "Z";
            return ss.str();
        }() + "\" }"
              "}";
    }

    // Helper function to generate a random collection name
    std::string generateRandomCollectionName()
    {
        return "benchmark_" + common_benchmark_helpers::generateRandomString(8);
    }

    // Helper function to create a collection for benchmarking
    void createBenchmarkCollection(std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> &conn, const std::string &collectionName)
    {
        try
        {
            // Drop collection if it exists
            conn->dropCollection(collectionName);

            // Create the collection
            conn->createCollection(collectionName);
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            throw; // Re-throw to fail the benchmark
        }
    }

    // Helper function to drop a collection after benchmarking
    void dropBenchmarkCollection(std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> &conn, const std::string &collectionName)
    {
        try
        {
            conn->dropCollection(collectionName);
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampError("Error dropping collection: " + std::string(e.what()));
        }
    }

    // Helper function to populate a collection with test documents
    void populateCollection(std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> &conn, const std::string &collectionName, int docCount)
    {
        auto collection = conn->getCollection(collectionName);

        // Insert the specified number of documents
        for (int i = 1; i <= docCount; ++i)
        {
            std::string name = "Name " + std::to_string(i);
            double value = i * 1.5;
            std::string description = common_benchmark_helpers::generateRandomString(50);

            std::string doc = generateTestDocument(i, name, value, description);
            collection->insertOne(doc);
        }
    }

    // Implementation of setupMongoDBConnection
    std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> setupMongoDBConnection(const std::string &collectionName, int docCount)
    {
        try
        {
            // Get database configuration
            auto dbConfig = getMongoDBConfig("dev_mongodb");

            // Get connection parameters
            std::string connStr = buildMongoDBConnectionString(dbConfig);
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Check if this collection has already been initialized
            bool needsInitialization = false;
            {
                std::lock_guard<std::mutex> lock(collectionMutex);
                auto it = collectionInitialized.find(collectionName);
                if (it == collectionInitialized.end() || !it->second)
                {
                    needsInitialization = true;
                    collectionInitialized[collectionName] = true;
                }
            }

            // Register the MongoDB driver and get a connection
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());
            auto conn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (needsInitialization)
            {
                // Create benchmark collection
                cpp_dbc::system_utils::logWithTimestampInfo("Creating and populating collection '" + collectionName + "' for the first time...");
                createBenchmarkCollection(conn, collectionName);

                // Populate collection with specified documents if docCount > 0
                if (docCount > 0)
                {
                    populateCollection(conn, collectionName, docCount);
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimestampInfo("Reusing existing collection '" + collectionName + "'");
            }

            return conn;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return nullptr;
        }
    }

#endif // USE_MONGODB

} // namespace mongodb_benchmark_helpers

namespace redis_benchmark_helpers
{

#if USE_REDIS
    // Track which key prefixes have been initialized to avoid recreating them
    static std::unordered_map<std::string, bool> keyPrefixInitialized;
    static std::mutex keyMutex;

    cpp_dbc::config::DatabaseConfig getRedisConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_benchmark_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("redis");
            dbConfig.setHost("localhost");
            dbConfig.setPort(6379);
            dbConfig.setDatabase("0");        // Redis uses database numbers
            dbConfig.setUsername("");         // Redis authentication
            dbConfig.setPassword("dsystems"); // Redis password
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("redis");
        dbConfig.setHost("localhost");
        dbConfig.setPort(6379);
        dbConfig.setDatabase("0");        // Redis uses database numbers
        dbConfig.setUsername("");         // Redis authentication
        dbConfig.setPassword("dsystems"); // Redis password
#endif

        return dbConfig;
    }

    // Build a Redis connection string from a DatabaseConfig
    std::string buildRedisConnectionString(const cpp_dbc::config::DatabaseConfig &dbConfig)
    {
        std::string host = dbConfig.getHost();
        int port = dbConfig.getPort();
        std::string database = dbConfig.getDatabase();

        // Build connection string for Redis
        // Format: cpp_dbc:redis://host:port/database
        std::string connStr = "cpp_dbc:redis://" + host + ":" + std::to_string(port) + "/" + database;

        return connStr;
    }

    bool canConnectToRedis()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getRedisConfig("dev_redis");

            // Get connection parameters
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Build connection string for Redis
            std::string connStr = buildRedisConnectionString(dbConfig);

            // Attempt to connect to Redis
            cpp_dbc::system_utils::logWithTimestampInfo("Attempting to connect to Redis with connection string: " + connStr);

            // Register the Redis driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Redis::RedisDriver>());

            auto conn = std::dynamic_pointer_cast<cpp_dbc::KVDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (!conn)
            {
                cpp_dbc::system_utils::logWithTimestampError("Redis connection returned null");
                return false;
            }

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimestampInfo("Redis connection successful!");

            // Try to ping the server
            std::string pingResult = conn->ping();
            cpp_dbc::system_utils::logWithTimestampInfo("Redis ping result: " + pingResult);

            // Close the connection
            conn->close();

            return true;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return false;
        }
    }

    // Helper function to get a Redis driver instance
    std::shared_ptr<cpp_dbc::Redis::RedisDriver> getRedisDriver()
    {
        return std::make_shared<cpp_dbc::Redis::RedisDriver>();
    }

    // Helper function to generate a random key
    std::string generateRandomKey(const std::string &prefix)
    {
        std::string randomPart = common_benchmark_helpers::generateRandomString(10);
        return prefix + ":" + randomPart;
    }

    // Helper function to populate Redis with test data
    void populateRedis(std::shared_ptr<cpp_dbc::KVDBConnection> &conn, const std::string &keyPrefix, int itemCount)
    {
        for (int i = 1; i <= itemCount; ++i)
        {
            // Create key name based on index and prefix
            std::string key = keyPrefix + ":" + std::to_string(i);

            // Generate a random value for the key
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            // Store the key-value pair
            conn->setString(key, value);
        }
    }

    // Helper function to clean up Redis keys
    void cleanupRedisKeys(std::shared_ptr<cpp_dbc::KVDBConnection> &conn, const std::string &keyPrefix)
    {
        try
        {
            // Scan for keys with the given prefix and delete them
            std::vector<std::string> keys = conn->scanKeys(keyPrefix + ":*");
            for (const auto &key : keys)
            {
                conn->deleteKey(key);
            }
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
        }
    }

    // Helper function to setup Redis connection
    std::shared_ptr<cpp_dbc::KVDBConnection> setupRedisConnection(const std::string &keyPrefix, int itemCount)
    {
        try
        {
            // Get database configuration
            auto dbConfig = getRedisConfig("dev_redis");

            // Get connection parameters
            std::string connStr = buildRedisConnectionString(dbConfig);
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Check if this key prefix has already been initialized
            bool needsInitialization = false;
            {
                std::lock_guard<std::mutex> lock(keyMutex);
                auto it = keyPrefixInitialized.find(keyPrefix);
                if (it == keyPrefixInitialized.end() || !it->second)
                {
                    needsInitialization = true;
                    keyPrefixInitialized[keyPrefix] = true;
                }
            }

            // Register the Redis driver and get a connection
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Redis::RedisDriver>());
            auto conn = std::dynamic_pointer_cast<cpp_dbc::KVDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (needsInitialization)
            {
                // Clean up any existing keys with this prefix
                cpp_dbc::system_utils::logWithTimestampInfo("Creating and populating Redis with prefix '" + keyPrefix + "' for the first time...");
                cleanupRedisKeys(conn, keyPrefix);

                // Populate Redis with specified number of items if itemCount > 0
                if (itemCount > 0)
                {
                    populateRedis(conn, keyPrefix, itemCount);
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimestampInfo("Reusing existing Redis keys with prefix '" + keyPrefix + "'");
            }

            return conn;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return nullptr;
        }
    }

#endif // USE_REDIS

} // namespace redis_benchmark_helpers

namespace scylladb_benchmark_helpers
{

#if USE_SCYLLADB
    // Track which tables have been initialized to avoid recreating them
    static std::unordered_map<std::string, bool> tableInitialized;
    static std::mutex tableMutex;

    /**
     * @brief Retrieve ScyllaDB connection configuration for the given database identifier.
     *
     * Loads configuration from the YAML config file when available; otherwise returns a sensible default
     * configuration for a local ScyllaDB instance.
     *
     * @param databaseName Name or key of the database configuration to fetch from the config file.
     * @return cpp_dbc::config::DatabaseConfig The resolved database configuration populated with host, port, keyspace (database), username, and password.
     */
    cpp_dbc::config::DatabaseConfig getScyllaDBConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_benchmark_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("scylladb");
            dbConfig.setHost("localhost");
            dbConfig.setPort(9042);
            dbConfig.setDatabase("test_keyspace");
            dbConfig.setUsername("cassandra");
            dbConfig.setPassword("cassandra");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("scylladb");
        dbConfig.setHost("localhost");
        dbConfig.setPort(9042);
        dbConfig.setDatabase("test_keyspace");
        dbConfig.setUsername("cassandra");
        dbConfig.setPassword("cassandra");
#endif

        return dbConfig;
    }

    /**
     * @brief Check whether a ScyllaDB instance is reachable and responsive using the "dev_scylladb" configuration.
     *
     * Uses the configured connection string and credentials, registers the ScyllaDB driver, opens a connection,
     * and executes a simple query against system.local to verify the server responds.
     *
     * @return `true` if ScyllaDB responded to the verification query, `false` otherwise.
     */
    bool canConnectToScyllaDB()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getScyllaDBConfig("dev_scylladb");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the ScyllaDB driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

            // Attempt to connect to ScyllaDB
            cpp_dbc::system_utils::logWithTimestampInfo("Attempting to connect to ScyllaDB with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimestampInfo("ScyllaDB connection successful!");

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT release_version FROM system.local");
            bool success = resultSet->next();

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return false;
        }
    }

    /**
     * @brief Create a ScyllaDB benchmark table with standard columns, dropping any existing table first.
     *
     * Attempts to drop a table named by @p tableName (errors during drop are logged and ignored), then creates
     * a table using CQL with columns: `id` (INT PRIMARY KEY), `name` (TEXT), `value` (DOUBLE), and `description` (TEXT).
     *
     * @param tableName Name of the ScyllaDB table to create (CQL identifier).
     *
     * @throws std::exception Propagates exceptions thrown by the connection when table creation fails.
     */
    void createScyllaDBBenchmarkTable(std::shared_ptr<cpp_dbc::ColumnarDBConnection> &conn, const std::string &tableName)
    {
        // Drop table if it exists
        try
        {
            conn->executeUpdate("DROP TABLE IF EXISTS " + tableName);
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampError("Error dropping table: " + std::string(e.what()));
        }

        // Create table with standard columns for benchmarks
        // ScyllaDB uses CQL syntax similar to Cassandra
        try
        {
            conn->executeUpdate(
                "CREATE TABLE " + tableName + " ("
                                              "id INT PRIMARY KEY, "
                                              "name TEXT, "
                                              "value DOUBLE, "
                                              "description TEXT"
                                              ")");
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            throw; // Re-throw to fail the test
        }
    }

    /**
     * @brief Insert a sequence of test rows into a ScyllaDB benchmark table.
     *
     * Inserts `rowCount` rows into `tableName`. Each inserted row has:
     * - `id`: integer from 1 to `rowCount`
     * - `name`: "Name <id>"
     * - `value`: `<id> * 1.5`
     * - `description`: a 50-character random alphanumeric string
     *
     * @param conn Active ColumnarDBConnection used to prepare and execute the insert statements.
     * @param tableName Target table name to populate.
     * @param rowCount Number of rows to insert; if less than or equal to zero, no rows are inserted.
     */
    void populateScyllaDBTable(std::shared_ptr<cpp_dbc::ColumnarDBConnection> &conn, const std::string &tableName, int rowCount)
    {
        // Prepare the insert statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");

        // Insert the specified number of rows
        for (int i = 1; i <= rowCount; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            pstmt->executeUpdate();
        }
    }

    /**
     * @brief Establishes a ScyllaDB connection and ensures a benchmark table exists.
     *
     * Obtains configuration, creates and returns a ColumnarDBConnection for the given table name.
     * If this is the first call for the specified table, the function will create the benchmark table
     * and, when `rowCount` is greater than zero, populate it with that many rows. Table initialization
     * is performed at most once per table name across concurrent callers.
     *
     * @param tableName Name of the benchmark table to create or reuse.
     * @param rowCount Number of rows to insert into the table when initializing; ignored if less than or equal to zero.
     * @return std::shared_ptr<cpp_dbc::ColumnarDBConnection> Active database connection, or `nullptr` if an error occurred.
     */
    std::shared_ptr<cpp_dbc::ColumnarDBConnection> setupScyllaDBConnection(const std::string &tableName, int rowCount)
    {
        try
        {
            // Get database configuration
            auto dbConfig = getScyllaDBConfig("dev_scylladb");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Check if this table has already been initialized
            bool needsInitialization = false;
            {
                std::lock_guard<std::mutex> lock(tableMutex);
                auto it = tableInitialized.find(tableName);
                if (it == tableInitialized.end() || !it->second)
                {
                    needsInitialization = true;
                    tableInitialized[tableName] = true;
                }
            }

            // Register the ScyllaDB driver and get a connection
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());
            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (needsInitialization)
            {
                // Create benchmark table
                cpp_dbc::system_utils::logWithTimestampInfo("Creating and populating table '" + tableName + "' for the first time...");
                createScyllaDBBenchmarkTable(conn, tableName);

                // Populate table with specified rows if rowCount > 0
                if (rowCount > 0)
                {
                    populateScyllaDBTable(conn, tableName, rowCount);
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimestampInfo("Reusing existing table '" + tableName + "'");
            }

            return conn;
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimestampException(e.what());
            return nullptr;
        }
    }

#endif // USE_SCYLLADB

} // namespace scylladb_benchmark_helpers