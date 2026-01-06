#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

// Include paths for using the library as an external dependency
#include <cpp_dbc/cpp_dbc.hpp>

// For convenience
using json = nlohmann::json;
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

// Include backward.hpp to check if libdw is enabled
#include <cpp_dbc/backward.hpp>
#include <cpp_dbc/common/system_utils.hpp>

void function3()
{
    std::cout << "Capturing call stack from function3..." << std::endl;
    auto frames = cpp_dbc::system_utils::captureCallStack(false);
    cpp_dbc::system_utils::printCallStack(frames);
}

void function2()
{
    function3();
}

void function1()
{
    function2();
}

int main(int argc, char *argv[])
{
    std::cout << "CPP_DBC Demo Application" << std::endl;
    std::cout << "------------------------" << std::endl;

    // Display available database drivers
    std::cout << "available database drivers:" << std::endl;

#if USE_MYSQL
    std::cout << "- MySQL" << std::endl;
#else
    std::cout << "- MySQL (disabled)" << std::endl;
#endif

#if USE_POSTGRESQL
    std::cout << "- PostgreSQL" << std::endl;
#else
    std::cout << "- PostgreSQL (disabled)" << std::endl;
#endif

#if USE_SQLITE
    std::cout << "- SQLITE" << std::endl;
#else
    std::cout << "- SQLITE (disabled)" << std::endl;
#endif
#if USE_FIREBIRD
    std::cout << "- Firebird" << std::endl;
#else
    std::cout << "- Firebird (disabled)" << std::endl;
#endif

#if USE_MONGODB
    std::cout << "- MongoDB" << std::endl;
#else
    std::cout << "- MongoDB (disabled)" << std::endl;
#endif

#if USE_REDIS
    std::cout << "- Redis" << std::endl;
#else
    std::cout << "- Redis (disabled)" << std::endl;
#endif

// Display libdw support status
#if BACKWARD_HAS_DW
    std::cout << "- libdw support: ENABLED" << std::endl;
#else
    std::cout << "- libdw support: DISABLED" << std::endl;
#endif

    // Test stack trace functionality
    std::cout << "\nTesting stack trace functionality:" << std::endl;
    function1();

    try
    {
        // Register available database drivers
#if USE_MYSQL
        std::cout << "Registering MySQL driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
#endif

#if USE_POSTGRESQL
        std::cout << "Registering PostgreSQL driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
#endif

#if USE_SQLITE
        std::cout << "Registering SQLite driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
#endif

#if USE_FIREBIRD
        std::cout << "Registering Firebird driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());
#endif

#if USE_MONGODB
        std::cout << "Registering MongoDB driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());
#endif

#if USE_REDIS
        std::cout << "Registering MongoDB driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Redis::RedisDBDriver>());
#endif

        std::cout << "Driver registration complete." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << e.what_s() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    // Process command line arguments
    std::vector<std::string> args(argv, argv + argc);

    if (args.size() > 1)
    {
        std::cout << "\nCommand line arguments:" << std::endl;
        for (size_t i = 1; i < args.size(); ++i)
        {
            std::cout << "  " << i << ": " << args[i] << std::endl;
        }
    }

    // Demonstrate nlohmann_json usage
    std::cout << "\nDemonstrating nlohmann_json usage:" << std::endl;

    // Create a JSON object with database configuration
    json db_config = {
        {"connections", {{{"name", "mysql_local"}, {"type", "mysql"}, {"host", "localhost"}, {"port", 3306}, {"user", "root"}, {"database", "test_db"}}, {{"name", "postgres_dev"}, {"type", "postgresql"}, {"host", "db.example.com"}, {"port", 5432}, {"user", "dev_user"}, {"database", "dev_db"}}}}};

    // Print the JSON object with pretty formatting
    std::cout << "Database configuration JSON:" << std::endl;
    std::cout << db_config.dump(4) << std::endl;

    // Access JSON values
    std::cout << "\nAccessing JSON values:" << std::endl;
    std::cout << "Number of connections: " << db_config["connections"].size() << std::endl;
    std::cout << "First connection name: " << db_config["connections"][0]["name"] << std::endl;
    std::cout << "Second connection type: " << db_config["connections"][1]["type"] << std::endl;

    // Modify JSON values
    db_config["connections"][0]["port"] = 3307;

    // Add a new connection
    json new_connection = {
        {"name", "sqlite_local"},
        {"type", "sqlite"},
        {"database", "local.db"}};
    db_config["connections"].push_back(new_connection);

    // Add Firebird connection
    json firebird_connection = {
        {"name", "firebird_local"},
        {"type", "firebird"},
        {"host", "localhost"},
        {"port", 3050},
        {"database", "/data/firebird/test.fdb"},
        {"user", "SYSDBA"},
        {"password", "masterkey"}};
    db_config["connections"].push_back(firebird_connection);

    // Add MongoDB connection
    json mongodb_connection = {
        {"name", "mongodb_local"},
        {"type", "mongodb"},
        {"connection_string", "mongodb://localhost:27017"},
        {"database", "test_db"}};
    db_config["connections"].push_back(mongodb_connection);

    // Print the modified JSON
    std::cout << "\nModified database configuration:" << std::endl;
    std::cout << db_config.dump(4) << std::endl;

    // Demonstrate yaml-cpp usage
    std::cout << "\nDemonstrating yaml-cpp usage:" << std::endl;

    // Create a YAML node with database configuration
    YAML::Node yaml_config;
    yaml_config["connections"] = YAML::Node(YAML::NodeType::Sequence);

    // Add MySQL connection
    YAML::Node mysql_conn;
    mysql_conn["name"] = "mysql_local";
    mysql_conn["type"] = "mysql";
    mysql_conn["host"] = "localhost";
    mysql_conn["port"] = 3306;
    mysql_conn["user"] = "root";
    mysql_conn["database"] = "test_db";
    yaml_config["connections"].push_back(mysql_conn);

    // Add PostgreSQL connection
    YAML::Node pg_conn;
    pg_conn["name"] = "postgres_dev";
    pg_conn["type"] = "postgresql";
    pg_conn["host"] = "db.example.com";
    pg_conn["port"] = 5432;
    pg_conn["user"] = "dev_user";
    pg_conn["database"] = "dev_db";
    yaml_config["connections"].push_back(pg_conn);

    // Print the YAML configuration
    std::cout << "Database configuration YAML:" << std::endl;
    std::cout << yaml_config << std::endl;

    // Access YAML values
    std::cout << "\nAccessing YAML values:" << std::endl;
    std::cout << "Number of connections: " << yaml_config["connections"].size() << std::endl;
    std::cout << "First connection name: " << yaml_config["connections"][0]["name"].as<std::string>() << std::endl;
    std::cout << "Second connection type: " << yaml_config["connections"][1]["type"].as<std::string>() << std::endl;

    // Modify YAML values
    yaml_config["connections"][0]["port"] = 3307;

    // Add a new connection
    YAML::Node sqlite_conn;
    sqlite_conn["name"] = "sqlite_local";
    sqlite_conn["type"] = "sqlite";
    sqlite_conn["database"] = "local.db";
    yaml_config["connections"].push_back(sqlite_conn);

    // Add Firebird connection
    YAML::Node firebird_conn;
    firebird_conn["name"] = "firebird_local";
    firebird_conn["type"] = "firebird";
    firebird_conn["host"] = "localhost";
    firebird_conn["port"] = 3050;
    firebird_conn["database"] = "/data/firebird/test.fdb";
    firebird_conn["user"] = "SYSDBA";
    firebird_conn["password"] = "masterkey";
    yaml_config["connections"].push_back(firebird_conn);

    // Add MongoDB connection
    YAML::Node mongodb_conn;
    mongodb_conn["name"] = "mongodb_local";
    mongodb_conn["type"] = "mongodb";
    mongodb_conn["connection_string"] = "mongodb://localhost:27017";
    mongodb_conn["database"] = "test_db";
    yaml_config["connections"].push_back(mongodb_conn);

    // Print the modified YAML
    std::cout << "\nModified database configuration:" << std::endl;
    std::cout << yaml_config << std::endl;

    // Test stack trace functionality
    std::cout << "\nTesting stack trace functionality:" << std::endl;
    auto frames = cpp_dbc::system_utils::captureCallStack(true);
    cpp_dbc::system_utils::printCallStack(frames);

    return 0;
}
