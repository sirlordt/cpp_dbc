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
        cpp_dbc::DriverManager::registerDriver("mysql",
                                               std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
#endif

#if USE_POSTGRESQL
        std::cout << "Registering PostgreSQL driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver("postgresql",
                                               std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
#endif

#if USE_SQLITE
        std::cout << "Registering SQLite driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver("sqlite",
                                               std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
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

    // Print the modified YAML
    std::cout << "\nModified database configuration:" << std::endl;
    std::cout << yaml_config << std::endl;

    // Test stack trace functionality
    std::cout << "\nTesting stack trace functionality:" << std::endl;
    auto frames = cpp_dbc::system_utils::captureCallStack(true);
    cpp_dbc::system_utils::printCallStack(frames);

    return 0;
}
