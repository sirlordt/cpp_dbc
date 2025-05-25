#include <iostream>
#include <string>
#include <vector>
#include <memory>

// Include paths for using the library as an external dependency
#include <cpp_dbc/cpp_dbc.hpp>
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

    return 0;
}
