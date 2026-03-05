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

#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <set>

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
            // Fallback: derive path from name so each config gets its own file
            dbConfig.setName(databaseName);
            dbConfig.setType("sqlite");
            dbConfig.setDatabase(useInMemory ? ":memory:" : "/tmp/" + databaseName + ".db");

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
        // Derive path from name so each config gets its own file
        dbConfig.setName(databaseName);
        dbConfig.setType("sqlite");
        dbConfig.setDatabase(useInMemory ? ":memory:" : "/tmp/" + databaseName + ".db");

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

    std::shared_ptr<cpp_dbc::SQLite::SQLiteDBDriver> getSQLiteDriver()
    {
        static std::shared_ptr<cpp_dbc::SQLite::SQLiteDBDriver> driver =
            std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>();
        return driver;
    }

    std::shared_ptr<cpp_dbc::RelationalDBConnection> getSQLiteConnection()
    {
        auto dbConfig = getSQLiteConfig("dev_sqlite");

        std::string connStr = dbConfig.createConnectionString();

        auto driver = getSQLiteDriver();
        cpp_dbc::DriverManager::registerDriver(driver);
        auto conn = cpp_dbc::DriverManager::getDBConnection(connStr, "", "");

        return std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(conn);
    }

    bool canConnectToSQLite()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getSQLiteConfig("dev_sqlite");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();

            // Register the SQLite driver singleton
            auto driver = getSQLiteDriver();
            cpp_dbc::DriverManager::registerDriver(driver);

            // Attempt to connect to SQLite
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to SQLite with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "SQLite connection successful!");

            // Verify the connection with ping()
            bool success = conn->ping();
            cpp_dbc::system_utils::logWithTimesMillis("TEST", std::string("SQLite ping ") + (success ? "successful!" : "returned false"));

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", std::string("SQLite connection error: ") + ex.what());
            return false;
        }
    }

    void cleanupSQLiteTestFiles(const std::string &dbPath, int waitSeconds)
    {
        // Guard: only run cleanup once per file per test process
        static std::mutex s_cleanupMutex;
        static std::set<std::string> s_cleanedPaths;

        {
            std::scoped_lock lock(s_cleanupMutex);
            if (s_cleanedPaths.count(dbPath) > 0)
            {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "=== SQLite cleanup skipped (already done for: " + dbPath + ") ===");
                return;
            }
            s_cleanedPaths.insert(dbPath);
        }

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "=== Cleaning up SQLite database files ===");
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Database path: " + dbPath);

        // Use error_code to avoid exceptions if files don't exist
        std::error_code ec;

        // Remove main database file
        std::filesystem::remove(dbPath, ec);
        if (!ec)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "  [OK] Removed: " + dbPath);
        }

        // Remove WAL (Write-Ahead Log) file
        std::string walPath = dbPath + "-wal";
        std::filesystem::remove(walPath, ec);
        if (!ec)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "  [OK] Removed: " + walPath);
        }

        // Remove shared memory file
        std::string shmPath = dbPath + "-shm";
        std::filesystem::remove(shmPath, ec);
        if (!ec)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "  [OK] Removed: " + shmPath);
        }

        // Remove journal file (used in non-WAL mode)
        std::string journalPath = dbPath + "-journal";
        std::filesystem::remove(journalPath, ec);
        if (!ec)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "  [OK] Removed: " + journalPath);
        }

        // Wait for filesystem buffers to flush and locks to release
        // This is CRITICAL under Helgrind/Valgrind where everything is slower
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Waiting " + std::to_string(waitSeconds) + " seconds for filesystem buffers to flush and locks to release...");
        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "=== SQLite cleanup completed ===");
    }

#endif

} // namespace sqlite_test_helpers