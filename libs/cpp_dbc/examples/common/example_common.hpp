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
 * @file example_common.hpp
 * @brief Common utilities for cpp_dbc examples
 *
 * This header-only file provides utilities for:
 * - Structured logging functions (logStep, logOk, logError, logData, logInfo, logMsg)
 * - Command-line argument parsing (--config=, --db=, --help)
 * - YAML configuration loading using nothrow pattern (expected<optional<T>, DBException>)
 * - Database configuration lookup with dev_* fallback
 * - Driver registration helpers
 * - Exit code constants for script integration
 */

#ifndef CPP_DBC_EXAMPLE_COMMON_HPP
#define CPP_DBC_EXAMPLE_COMMON_HPP

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/core/db_expected.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#ifdef USE_CPP_YAML
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif

// Driver includes - conditional based on build flags
#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif
#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif
#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif
#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif
#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace cpp_dbc::examples
{

    // ============================================================================
    // Exit Codes
    // ============================================================================

    /**
     * @brief Standard exit codes for examples
     *
     * These exit codes allow scripts to detect why an example terminated:
     * - EXIT_OK_ (0): Example completed successfully
     * - EXIT_ERROR_ (1): Example failed due to runtime error
     * - EXIT_DRIVER_NOT_ENABLED_ (100): Required driver not enabled at compile time
     *
     * Usage in scripts:
     *   if [ $? -eq 100 ]; then
     *       echo "Driver not enabled - skipping"
     *   fi
     *
     * Note: We use trailing underscore to avoid conflicts with
     * EXIT_SUCCESS/EXIT_FAILURE macros from <cstdlib>
     */
    constexpr int EXIT_OK_ = 0;
    constexpr int EXIT_ERROR_ = 1;
    constexpr int EXIT_DRIVER_NOT_ENABLED_ = 100;

    // ============================================================================
    // Structured Logging Functions
    // ============================================================================

    /**
     * @brief Structured logging functions using system_utils::logWithTimestamp
     *
     * All output goes through cpp_dbc::system_utils::logWithTimestamp for:
     * - Thread-safe output with mutex protection
     * - Consistent timestamp formatting
     * - Structured markers for LLM verification
     *
     * Usage:
     *   logStep("Connecting to database...");
     *   logOk("Connected successfully");
     *   logData("Row: id=1, name='John'");
     *   logError("Connection failed: " + e.what_s());
     *   logInfo("Using default config");
     *   logMsg("========================================");
     */

    /** @brief Log a message without marker (for headers, separators) */
    inline void logMsg(const std::string &message)
    {
        cpp_dbc::system_utils::logWithTimestamp("", message);
    }

    /** @brief Log a STEP message - used for operation progress */
    inline void logStep(const std::string &message)
    {
        cpp_dbc::system_utils::logWithTimestamp("[STEP]", message);
    }

    /** @brief Log an OK message - used for successful operations */
    inline void logOk(const std::string &message)
    {
        cpp_dbc::system_utils::logWithTimestamp("[OK]", message);
    }

    /** @brief Log an ERROR message - used for failed operations */
    inline void logError(const std::string &message)
    {
        cpp_dbc::system_utils::logWithTimestamp("[ERROR]", message);
    }

    /** @brief Log a DATA message - used for displaying query results */
    inline void logData(const std::string &message)
    {
        cpp_dbc::system_utils::logWithTimestamp("[DATA]", message);
    }

    /** @brief Log an INFO message - used for informational messages */
    inline void logInfo(const std::string &message)
    {
        cpp_dbc::system_utils::logWithTimestamp("[INFO]", message);
    }

    // ============================================================================
    // Path Utilities
    // ============================================================================

    /**
     * @brief Get the default config file path (same directory as executable)
     * @return Path to example_config.yml
     */
    inline std::string getDefaultConfigPath()
    {
        return cpp_dbc::system_utils::getExecutablePath() + "example_config.yml";
    }

    // ============================================================================
    // Command Line Argument Parsing
    // ============================================================================

    /**
     * @brief Configuration parsed from command line arguments
     */
    struct ExampleArgs
    {
        std::string configPath; ///< Path to YAML config file (from --config= or default)
        std::string dbName;     ///< Database config name (from --db= or empty for dev_* default)
        bool showHelp = false;  ///< True if --help was specified
    };

    /**
     * @brief Parse command line arguments
     *
     * Supported arguments:
     *   --config=<path>  : Path to YAML configuration file
     *   --db=<name>      : Database configuration name (e.g., dev_mysql, test_postgresql)
     *   --help, -h       : Show help message
     *
     * @param argc Argument count from main()
     * @param argv Argument values from main()
     * @return Parsed configuration
     */
    inline ExampleArgs parseArgs(int argc, char *argv[])
    {
        ExampleArgs args;
        args.configPath = getDefaultConfigPath();

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                args.showHelp = true;
            }
            else if (arg.rfind("--config=", 0) == 0)
            {
                args.configPath = arg.substr(9);
            }
            else if (arg.rfind("--db=", 0) == 0)
            {
                args.dbName = arg.substr(5);
            }
        }

        return args;
    }

    // ============================================================================
    // Configuration Loading (nothrow pattern)
    // ============================================================================

    /**
     * @brief Load YAML configuration file using nothrow pattern
     *
     * This demonstrates the cpp_dbc nothrow pattern:
     * - Returns expected<optional<T>, DBException>
     * - has_value() + value().has_value() → Config loaded successfully
     * - has_value() + !value().has_value() → File not found (not an error)
     * - !has_value() → Real error occurred (DBException with code and callstack)
     *
     * @param path Path to YAML configuration file
     * @return expected containing optional<DatabaseConfigManager> or DBException
     */
    inline cpp_dbc::expected<std::optional<cpp_dbc::config::DatabaseConfigManager>, cpp_dbc::DBException>
    loadConfig([[maybe_unused]] const std::string &path)
    {
#ifdef USE_CPP_YAML
        // Check if file exists - not an error, just return empty optional
        if (!std::filesystem::exists(path))
        {
            return std::optional<cpp_dbc::config::DatabaseConfigManager>(std::nullopt);
        }

        try
        {
            auto configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(path);
            return std::optional<cpp_dbc::config::DatabaseConfigManager>(std::move(configManager));
        }
        catch (const cpp_dbc::DBException &e)
        {
            // Re-wrap as unexpected to return via expected
            return cpp_dbc::unexpected<cpp_dbc::DBException>(e);
        }
        catch (const std::exception &e)
        {
            // Convert std::exception to DBException
            return cpp_dbc::unexpected<cpp_dbc::DBException>(
                cpp_dbc::DBException("CFGLOAD3R7K9", std::string("Failed to load configuration: ") + e.what(),
                                     cpp_dbc::system_utils::captureCallStack()));
        }
#else
        // YAML support not enabled - this is an error, not just missing file
        return cpp_dbc::unexpected<cpp_dbc::DBException>(
            cpp_dbc::DBException("YAMLNOTENA8B", "YAML support not enabled. Build with -DUSE_CPP_YAML=ON",
                                 cpp_dbc::system_utils::captureCallStack()));
#endif
    }

    /**
     * @brief Get database configuration by name or type with dev_* fallback
     *
     * Uses nothrow pattern:
     * - has_value() + value().has_value() → Config found
     * - has_value() + !value().has_value() → Config not found (not an error)
     * - !has_value() → Error in lookup
     *
     * If dbName is specified, looks up that exact name.
     * If dbName is empty, looks for "dev_<dbType>" as default.
     *
     * @param manager Configuration manager
     * @param dbName Explicit database name (from --db=) or empty
     * @param dbType Database type for dev_* fallback (e.g., "mysql", "redis")
     * @return expected containing optional<DatabaseConfig> or DBException
     */
    inline cpp_dbc::expected<std::optional<cpp_dbc::config::DatabaseConfig>, cpp_dbc::DBException>
    getDbConfig(
        const cpp_dbc::config::DatabaseConfigManager &manager,
        const std::string &dbName,
        const std::string &dbType)
    {
        std::string targetName = dbName;

        if (targetName.empty())
        {
            // Use dev_* default
            targetName = "dev_" + dbType;
            logInfo("Using default: " + targetName + " (use --db=<name> to override)");
        }

        auto dbConfigOpt = manager.getDatabaseByName(targetName);
        if (dbConfigOpt)
        {
            return std::optional<cpp_dbc::config::DatabaseConfig>(dbConfigOpt->get());
        }

        // If not found by name and dbName was explicitly provided, that's not found
        if (!dbName.empty())
        {
            logError("Database configuration '" + dbName + "' not found");
            return std::optional<cpp_dbc::config::DatabaseConfig>(std::nullopt);
        }

        // Try to find any database of the requested type
        auto databases = manager.getDatabasesByType(dbType);
        if (!databases.empty())
        {
            logInfo("Falling back to first " + dbType + " config: " + databases[0].getName());
            return std::optional<cpp_dbc::config::DatabaseConfig>(databases[0]);
        }

        logInfo("No database configuration found for type: " + dbType);
        return std::optional<cpp_dbc::config::DatabaseConfig>(std::nullopt);
    }

    // ============================================================================
    // Help Output
    // ============================================================================

    /**
     * @brief Print usage help for an example
     *
     * @param exampleName Name of the example program
     * @param dbTypes Comma-separated list of supported database types
     */
    inline void printHelp(const std::string &exampleName, const std::string &dbTypes)
    {
        std::cout << "Usage: " + exampleName + " [OPTIONS]" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --config=<path>  Path to YAML configuration file" << std::endl;
        std::cout << "                   Default: ./example_config.yml (same dir as executable)" << std::endl;
        std::cout << "  --db=<name>      Database configuration name from YAML" << std::endl;
        std::cout << "                   Default: dev_<type> (e.g., dev_mysql, dev_redis)" << std::endl;
        std::cout << "  --help, -h       Show this help message" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Supported database types: " + dbTypes << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " + exampleName << std::endl;
        std::cout << "  " + exampleName + " --db=test_mysql" << std::endl;
        std::cout << "  " + exampleName + " --config=/path/to/config.yml --db=prod_postgresql" << std::endl;
    }

    // ============================================================================
    // Driver Registration
    // ============================================================================

    /**
     * @brief Register all available database drivers
     *
     * Registers drivers based on build-time flags (USE_MYSQL, USE_POSTGRESQL, etc.)
     */
    inline void registerAllDrivers()
    {
#if USE_MYSQL
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
#endif
#if USE_POSTGRESQL
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
#endif
#if USE_SQLITE
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
#endif
#if USE_FIREBIRD
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());
#endif
#if USE_MONGODB
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());
#endif
#if USE_REDIS
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Redis::RedisDriver>());
#endif
#if USE_SCYLLADB
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());
#endif
    }

    /**
     * @brief Register a specific driver by type
     *
     * @param dbType Database type (mysql, postgresql, sqlite, firebird, mongodb, redis, scylladb)
     * @return true if driver was registered, false if not available
     */
    inline bool registerDriver(const std::string &dbType)
    {
#if USE_MYSQL
        if (dbType == "mysql")
        {
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
            return true;
        }
#endif
#if USE_POSTGRESQL
        if (dbType == "postgresql")
        {
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
            return true;
        }
#endif
#if USE_SQLITE
        if (dbType == "sqlite")
        {
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
            return true;
        }
#endif
#if USE_FIREBIRD
        if (dbType == "firebird")
        {
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());
            return true;
        }
#endif
#if USE_MONGODB
        if (dbType == "mongodb")
        {
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());
            return true;
        }
#endif
#if USE_REDIS
        if (dbType == "redis")
        {
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Redis::RedisDriver>());
            return true;
        }
#endif
#if USE_SCYLLADB
        if (dbType == "scylladb")
        {
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());
            return true;
        }
#endif
        (void)dbType; // Suppress unused parameter warning
        return false;
    }

    /**
     * @brief Check if a database type is available (built with support)
     *
     * @param dbType Database type to check
     * @return true if driver is available
     */
    inline bool isDriverAvailable(const std::string &dbType)
    {
#if USE_MYSQL
        if (dbType == "mysql")
            return true;
#endif
#if USE_POSTGRESQL
        if (dbType == "postgresql")
            return true;
#endif
#if USE_SQLITE
        if (dbType == "sqlite")
            return true;
#endif
#if USE_FIREBIRD
        if (dbType == "firebird")
            return true;
#endif
#if USE_MONGODB
        if (dbType == "mongodb")
            return true;
#endif
#if USE_REDIS
        if (dbType == "redis")
            return true;
#endif
#if USE_SCYLLADB
        if (dbType == "scylladb")
            return true;
#endif
        (void)dbType; // Suppress unused parameter warning
        return false;
    }

    // ============================================================================
    // Firebird Database Auto-Creation
    // ============================================================================

#if USE_FIREBIRD
    /**
     * @brief Try to create Firebird database if it doesn't exist
     *
     * This function:
     * 1. Tries to connect to the database to see if it exists
     * 2. If connection fails, attempts to create the database using Firebird C API
     * 3. Provides helpful error messages if creation fails
     *
     * @param dbConfig Database configuration with connection parameters
     * @return true if database exists or was created successfully, false otherwise
     */
    inline bool tryCreateFirebirdDatabase(const cpp_dbc::config::DatabaseConfig &dbConfig)
    {
        // Use explicit namespace to avoid ambiguity with other logMsg functions
        using cpp_dbc::examples::logError;
        using cpp_dbc::examples::logInfo;
        using cpp_dbc::examples::logOk;

        try
        {
            // Get connection parameters
            std::string type = dbConfig.getType();
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string database = dbConfig.getDatabase();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Build connection string for cpp_dbc
            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

            // First, try to connect to see if database already exists
            try
            {
                auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                    cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                logInfo("Firebird database exists and connection successful");
                conn->close();
                return true;
            }
            catch (const std::exception &)
            {
                // Database doesn't exist, try to create it
                logInfo("Database doesn't exist, attempting to create it...");
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
            std::string createDbSql = "CREATE DATABASE '" + fbConnStr + "' "
                                                                        "USER '" +
                                      username + "' "
                                                 "PASSWORD '" +
                                      password + "' "
                                                 "PAGE_SIZE 4096 "
                                                 "DEFAULT CHARACTER SET UTF8";

            // Log sanitized version (without password)
            std::string sanitizedSql = "CREATE DATABASE '" + fbConnStr + "' "
                                                                         "USER '" +
                                       username + "' "
                                                  "PASSWORD '***' "
                                                  "PAGE_SIZE 4096 "
                                                  "DEFAULT CHARACTER SET UTF8";
            logInfo("Executing: " + sanitizedSql);

            ISC_STATUS_ARRAY status;
            isc_db_handle db = 0;
            isc_tr_handle tr = 0;

            // Execute CREATE DATABASE using isc_dsql_execute_immediate
            if (isc_dsql_execute_immediate(status, &db, &tr, 0, createDbSql.c_str(), SQL_DIALECT_V6, nullptr))
            {
                std::string errorMsg = cpp_dbc::Firebird::interpretStatusVector(status);
                logError("Failed to create database: " + errorMsg);
                logInfo("");
                logInfo("To fix this, you may need to:");
                logInfo("1. Ensure the directory exists and is writable by the Firebird server");
                logInfo("2. Configure Firebird to allow database creation in the target directory");
                logInfo("   Edit /etc/firebird/3.0/firebird.conf (or similar path)");
                logInfo("   Set: DatabaseAccess = Full");
                logInfo("3. Restart Firebird: sudo systemctl restart firebird3.0");
                logInfo("");
                logInfo("Alternatively, create the database manually:");
                logInfo("   isql-fb -user " + username + " -password <your_password>");
                logInfo("   SQL> CREATE DATABASE '" + database + "';");
                logInfo("   SQL> quit;");
                return false;
            }

            logOk("Firebird database created successfully!");

            // Detach from the newly created database
            if (db)
            {
                isc_detach_database(status, &db);
            }

            return true;
        }
        catch (const std::exception &e)
        {
            logError("Database creation error: " + std::string(e.what()));
            return false;
        }
    }
#endif

} // namespace cpp_dbc::examples

#endif // CPP_DBC_EXAMPLE_COMMON_HPP
