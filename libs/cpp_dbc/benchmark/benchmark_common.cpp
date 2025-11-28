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

    // Implementation of createBenchmarkTable function
    void createBenchmarkTable(std::shared_ptr<cpp_dbc::Connection> &conn, const std::string &tableName)
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
            std::cerr << "Error creating table: " << e.what() << std::endl;
            throw; // Re-throw to fail the test
        }
    }

    // Implementation of dropBenchmarkTable function
    void dropBenchmarkTable(std::shared_ptr<cpp_dbc::Connection> &conn, const std::string &tableName)
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

    // Implementation of populateTable function
    void populateTable(std::shared_ptr<cpp_dbc::Connection> &conn, const std::string &tableName, int rowCount)
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
            cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

            // Attempt to connect to MySQL
            std::cout << "Attempting to connect to MySQL with connection string: " << connStr << std::endl;

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

#endif // USE_MYSQL

} // namespace mysql_benchmark_helpers

namespace postgresql_benchmark_helpers
{

#if USE_POSTGRESQL

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
            cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

            // Attempt to connect to PostgreSQL
            std::cout << "Attempting to connect to PostgreSQL with connection string: " << connStr << std::endl;

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

#endif // USE_POSTGRESQL

} // namespace postgresql_benchmark_helpers

namespace sqlite_benchmark_helpers
{

#if USE_SQLITE

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
            dbConfig.setDatabase(":memory:");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("sqlite");
        dbConfig.setDatabase(":memory:");
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
            cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());

            // Attempt to connect to SQLite
            std::cout << "Attempting to connect to SQLite with connection string: " << connStr << std::endl;

            auto conn = cpp_dbc::DriverManager::getConnection(connStr, "", "");

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

#endif // USE_SQLITE

} // namespace sqlite_benchmark_helpers