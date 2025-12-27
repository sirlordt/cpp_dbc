#include <iostream>
#include <memory>
#include <string>
#include <cpp_dbc/cpp_dbc.hpp>

#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif

using namespace cpp_dbc;

int main()
{
// Initialize the necessary drivers
#if USE_MYSQL
    {
        auto mysqlDriver = std::make_shared<MySQL::MySQLDBDriver>();
        std::shared_ptr<DBDriver> driver = std::static_pointer_cast<DBDriver>(mysqlDriver);
        DriverManager::registerDriver("mysql", driver);
    }
#endif

#if USE_POSTGRESQL
    {
        auto pgDriver = std::make_shared<PostgreSQL::PostgreSQLDBDriver>();
        std::shared_ptr<DBDriver> driver = std::static_pointer_cast<DBDriver>(pgDriver);
        DriverManager::registerDriver("postgresql", driver);
    }
#endif

#if USE_SQLITE
    {
        auto sqliteDriver = std::make_shared<SQLite::SQLiteDBDriver>();
        std::shared_ptr<DBDriver> driver = std::static_pointer_cast<DBDriver>(sqliteDriver);
        DriverManager::registerDriver("sqlite", driver);
    }
#endif

    std::cout << "Connection URL Examples:" << std::endl;
    std::cout << "=======================" << std::endl;

    try
    {
#if USE_MYSQL
        // Create a MySQL connection
        auto mysqlConn = DriverManager::getDBConnection(
            "cpp_dbc:mysql://localhost:3306/test_db",
            "user", "password");

        std::cout << "MySQL Connection URL: " << mysqlConn->getURL() << std::endl;
#else
        std::cout << "MySQL support is not enabled in this build" << std::endl;
#endif

#if USE_POSTGRESQL
        // Create a PostgreSQL connection
        auto pgConn = DriverManager::getDBConnection(
            "cpp_dbc:postgresql://localhost:5432/test_db",
            "user", "password");

        std::cout << "PostgreSQL Connection URL: " << pgConn->getURL() << std::endl;
#else
        std::cout << "PostgreSQL support is not enabled in this build" << std::endl;
#endif

#if USE_SQLITE
        // Create a SQLite connection
        auto sqliteConn = DriverManager::getDBConnection(
            "cpp_dbc:sqlite:///tmp/test.db",
            "", "");

        std::cout << "SQLite Connection URL: " << sqliteConn->getURL() << std::endl;

        // Also test with in-memory database
        auto sqliteMemConn = DriverManager::getDBConnection(
            "cpp_dbc:sqlite://:memory:",
            "", "");

        std::cout << "SQLite In-Memory Connection URL: " << sqliteMemConn->getURL() << std::endl;
#else
        std::cout << "SQLite support is not enabled in this build" << std::endl;
#endif
    }
    catch (const DBException &e)
    {
        std::cerr << "Error: " << e.what_s() << std::endl;
        e.printCallStack();
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}