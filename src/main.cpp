#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

// Include paths for using the library as an external dependency
#include <cpp_dbc/cpp_dbc.hpp>

// For convenience
using json = nlohmann::json;
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif

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

    try
    {
        // Register available database drivers
#if USE_MYSQL
        std::cout << "Registering MySQL driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver("mysql",
                                               std::make_shared<cpp_dbc::MySQL::MySQLDriver>());
#endif

#if USE_POSTGRESQL
        std::cout << "Registering PostgreSQL driver..." << std::endl;
        cpp_dbc::DriverManager::registerDriver("postgresql",
                                               std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());
#endif

        std::cout << "Driver registration complete." << std::endl;
    }
    catch (const cpp_dbc::SQLException &e)
    {
        std::cerr << "SQL Error: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
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

    return 0;
}
