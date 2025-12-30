#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <iostream>

int main()
{
    try
    {
        std::cout << "cpp_dbc example application" << std::endl;

        // Create a ConnectionPool instance
        cpp_dbc::RelationalDBConnectionPool pool(
            "mysql://localhost:3306/testdb", // URL
            "username",                      // Username
            "password",                      // Password
            {},                              // Options
            5,                               // Initial size
            20,                              // Max size
            3                                // Min idle
        );

        // Print library information
        std::cout << "cpp_dbc library successfully linked!" << std::endl;
        std::cout << "Connection pool created with "
                  << pool.getTotalDBConnectionCount() << " connections" << std::endl;

        // You would normally connect to a database here
        // For example:
        // auto conn = pool.getDBConnection();

        std::cout << "Example completed successfully" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}