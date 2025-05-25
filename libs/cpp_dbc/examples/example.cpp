// CPPDBC_Example_2.cpp
// Example of using the CPPDBC library

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include <iostream>
#include <memory>

// Function to demonstrate common usage with any database
void performDatabaseOperations(std::shared_ptr<cpp_dbc::Connection> conn)
{
    try
    {
        // Create a table (if it doesn't exist)
        conn->executeUpdate(
            "CREATE TABLE IF NOT EXISTS employees ("
            "id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "salary DOUBLE, "
            "hire_date DATE"
            ")");

        // Insert data using a prepared statement
        auto prepStmt = conn->prepareStatement(
            "INSERT INTO employees (id, name, salary, hire_date) VALUES (?, ?, ?, ?)");

        prepStmt->setInt(1, 101);
        prepStmt->setString(2, "John Doe");
        prepStmt->setDouble(3, 75000.50);
        prepStmt->setString(4, "2023-05-15"); // Date as string
        int rowsAffected = prepStmt->executeUpdate();

        std::cout << rowsAffected << " row(s) inserted." << std::endl;

        // Query data
        auto resultSet = conn->executeQuery("SELECT * FROM employees");

        // Display column names
        auto columnNames = resultSet->getColumnNames();
        for (const auto &column : columnNames)
        {
            std::cout << column << "\t";
        }
        std::cout << std::endl;

        // Display data
        while (resultSet->next())
        {
            int id = resultSet->getInt("id");
            std::string name = resultSet->getString("name");
            double salary = resultSet->getDouble("salary");
            std::string hireDate = resultSet->getString("hire_date");

            std::cout << id << "\t" << name << "\t" << salary << "\t" << hireDate << std::endl;
        }

        // Transaction example
        conn->setAutoCommit(false);

        try
        {
            conn->executeUpdate("UPDATE employees SET salary = 80000 WHERE id = 101");
            conn->executeUpdate("INSERT INTO employees (id, name, salary) VALUES (102, 'Jane Smith', 65000)");

            // Commit the transaction
            conn->commit();
            std::cout << "Transaction committed successfully." << std::endl;
        }
        catch (const cpp_dbc::SQLException &e)
        {
            // Rollback in case of error
            conn->rollback();
            std::cout << "Transaction rolled back: " << e.what() << std::endl;
        }

        // Restore auto-commit mode
        conn->setAutoCommit(true);
    }
    catch (const cpp_dbc::SQLException &e)
    {
        std::cerr << "SQL Error: " << e.what() << std::endl;
    }
}

int main()
{
    try
    {
        // Register database drivers
#if USE_MYSQL
        cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());
#endif
#if USE_POSTGRESQL
        cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());
#endif

        // Example with MySQL
        std::cout << "Connecting to MySQL..." << std::endl;
        auto mysqlConn = cpp_dbc::DriverManager::getConnection(
            "cpp_dbc:mysql://localhost:3306/testdb",
            "username",
            "password");

        std::cout << "MySQL Operations:" << std::endl;
        performDatabaseOperations(mysqlConn);
        mysqlConn->close();

        // Example with PostgreSQL
        std::cout << "\nConnecting to PostgreSQL..." << std::endl;
        auto pgConn = cpp_dbc::DriverManager::getConnection(
            "cpp_dbc:postgresql://localhost:5432/testdb",
            "username",
            "password");

        std::cout << "PostgreSQL Operations:" << std::endl;
        performDatabaseOperations(pgConn);
        pgConn->close();
    }
    catch (const cpp_dbc::SQLException &e)
    {
        std::cerr << "Database Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
