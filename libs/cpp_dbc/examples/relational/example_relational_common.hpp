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
 * @file example_relational_common.hpp
 * @brief Shared operations for relational database examples (DRY principle)
 *
 * This header contains common database operations that can be reused across
 * MySQL, PostgreSQL, SQLite, and Firebird examples. Following DRY principle,
 * the actual DB operations are defined here once and called by DB-specific
 * example main() functions.
 *
 * DB-specific examples only need to:
 * 1. Check driver availability (return 100 if not enabled)
 * 2. Parse args, load config, connect
 * 3. Call shared operation functions from this header
 * 4. Cleanup
 */

#ifndef CPP_DBC_EXAMPLE_RELATIONAL_COMMON_HPP
#define CPP_DBC_EXAMPLE_RELATIONAL_COMMON_HPP

#include "../common/example_common.hpp"
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <iomanip>
#include <sstream>

namespace cpp_dbc::examples::relational
{

    // ============================================================================
    // Result Set Display Utilities
    // ============================================================================

    /**
     * @brief Print a result set in tabular format
     *
     * @param rs The result set to print
     * @param maxRows Maximum number of rows to print (0 = unlimited)
     * @return Number of rows printed
     */
    inline int printResultSet(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs,
                              int maxRows = 0)
    {
        auto columnNames = rs->getColumnNames();

        // Print header
        std::ostringstream headerOss;
        for (const auto &column : columnNames)
        {
            headerOss << std::setw(15) << std::left << column;
        }
        logData(headerOss.str());

        // Print separator
        std::ostringstream sepOss;
        for (size_t i = 0; i < columnNames.size(); ++i)
        {
            sepOss << std::string(15, '-');
        }
        logData(sepOss.str());

        // Print rows
        int rowCount = 0;
        while (rs->next())
        {
            if (maxRows > 0 && rowCount >= maxRows)
            {
                logInfo("... (more rows not shown)");
                break;
            }

            std::ostringstream rowOss;
            for (const auto &column : columnNames)
            {
                rowOss << std::setw(15) << std::left << rs->getString(column);
            }
            logData(rowOss.str());
            ++rowCount;
        }
        logOk(std::to_string(rowCount) + " row(s) returned");
        return rowCount;
    }

    // ============================================================================
    // Basic CRUD Operations Demo
    // ============================================================================

    /**
     * @brief Perform basic CRUD operations demonstrating the library
     *
     * This function demonstrates:
     * - CREATE TABLE
     * - INSERT with prepared statements
     * - SELECT and result iteration
     * - UPDATE
     * - Transactions (commit/rollback)
     * - DELETE (cleanup)
     *
     * @param conn Active database connection
     * @param dbType Database type for logging purposes
     */
    inline void performCrudOperations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn,
                                      const std::string &dbType)
    {
        log("");
        log("--- Basic CRUD Operations (" + dbType + ") ---");
        log("");

        // ===== Create Table =====
        logStep("Creating table 'employees'...");
        // Use database-specific syntax for floating point type
        std::string salaryType = (dbType == "postgresql" || dbType == "firebird") ? "DOUBLE PRECISION" : "DOUBLE";
        conn->executeUpdate(
            "CREATE TABLE IF NOT EXISTS employees ("
            "id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "salary " + salaryType + ", "
            "hire_date DATE"
            ")");
        logOk("Table 'employees' created/verified");

        // ===== Clear Test Data =====
        logStep("Clearing existing test data...");
        conn->executeUpdate("DELETE FROM employees WHERE id IN (101, 102)");
        logOk("Test data cleared");

        // ===== Insert with Prepared Statement =====
        logStep("Inserting data with prepared statement...");
        auto prepStmt = conn->prepareStatement(
            "INSERT INTO employees (id, name, salary, hire_date) VALUES (?, ?, ?, ?)");

        prepStmt->setInt(1, 101);
        prepStmt->setString(2, "John Doe");
        prepStmt->setDouble(3, 75000.50);
        prepStmt->setDate(4, "2023-05-15");
        auto rowsAffected = prepStmt->executeUpdate();
        logData("Inserted: id=101, name='John Doe', salary=75000.50, hire_date='2023-05-15'");
        logOk(std::to_string(rowsAffected) + " row(s) inserted");

        // ===== Query =====
        logStep("Querying employees...");
        auto resultSet = conn->executeQuery("SELECT * FROM employees WHERE id = 101");

        auto columnNames = resultSet->getColumnNames();
        std::string columnsStr = "Columns: ";
        for (size_t i = 0; i < columnNames.size(); ++i)
        {
            columnsStr += columnNames[i];
            if (i < columnNames.size() - 1)
                columnsStr += ", ";
        }
        logInfo(columnsStr);

        int rowCount = 0;
        while (resultSet->next())
        {
            int id = resultSet->getInt("id");
            std::string name = resultSet->getString("name");
            double salary = resultSet->getDouble("salary");
            std::string hireDate = resultSet->getString("hire_date");

            std::ostringstream oss;
            oss << "Row: id=" << id
                << ", name='" << name << "'"
                << ", salary=" << std::fixed << std::setprecision(2) << salary
                << ", hire_date='" << hireDate << "'";
            logData(oss.str());
            ++rowCount;
        }
        logOk("Query returned " + std::to_string(rowCount) + " row(s)");

        // ===== Transaction Demo =====
        log("");
        log("--- Transaction Demo ---");
        log("");

        logStep("Beginning transaction...");
        conn->beginTransaction();
        logOk("Transaction started");

        try
        {
            logStep("Updating salary for id=101...");
            conn->executeUpdate("UPDATE employees SET salary = 80000 WHERE id = 101");
            logData("Updated: id=101, salary=80000");

            logStep("Inserting new employee id=102...");
            conn->executeUpdate("INSERT INTO employees (id, name, salary, hire_date) VALUES (102, 'Jane Smith', 65000, '2023-06-01')");
            logData("Inserted: id=102, name='Jane Smith', salary=65000, hire_date='2023-06-01'");

            logStep("Committing transaction...");
            conn->commit();
            logOk("Transaction committed successfully");
        }
        catch (const cpp_dbc::DBException &e)
        {
            logError("Transaction failed: " + e.what_s());
            logStep("Rolling back transaction...");
            conn->rollback();
            logOk("Transaction rolled back");
        }

        // Ensure auto-commit is restored after transaction
        conn->setAutoCommit(true);

        // ===== Verify Final Data =====
        logStep("Verifying final data...");
        resultSet = conn->executeQuery("SELECT * FROM employees WHERE id IN (101, 102) ORDER BY id");
        rowCount = 0;
        while (resultSet->next())
        {
            std::ostringstream oss;
            oss << "Row: id=" << resultSet->getInt("id")
                << ", name='" << resultSet->getString("name") << "'"
                << ", salary=" << std::fixed << std::setprecision(2) << resultSet->getDouble("salary");
            logData(oss.str());
            ++rowCount;
        }
        logOk("Verification complete, " + std::to_string(rowCount) + " row(s) found");

        // ===== Cleanup =====
        logStep("Cleaning up test data...");
        conn->executeUpdate("DELETE FROM employees WHERE id IN (101, 102)");
        logOk("Test data cleaned up");
    }

    // ============================================================================
    // Connection Pool Demo Operations
    // ============================================================================

    /**
     * @brief Demonstrate connection pool usage
     *
     * @param pool Active connection pool
     * @param dbType Database type for logging purposes
     */
    inline void performConnectionPoolDemo(std::shared_ptr<cpp_dbc::RelationalDBConnectionPool> pool,
                                          const std::string &dbType)
    {
        log("");
        log("--- Connection Pool Demo (" + dbType + ") ---");
        log("");

        logStep("Getting connection from pool...");
        auto conn = pool->getRelationalDBConnection();
        logOk("Connection acquired");

        logStep("Executing simple query...");
        auto rs = conn->executeQuery("SELECT 1 AS test_value");
        if (rs->next())
        {
            logData("Result: " + rs->getString("test_value"));
        }
        logOk("Query executed successfully");

        logStep("Returning connection to pool...");
        conn->returnToPool();
        logOk("Connection returned");

        logStep("Pool statistics:");
        logInfo("  Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logInfo("  Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
    }

    // ============================================================================
    // Common Main Function Pattern
    // ============================================================================

    /**
     * @brief Common setup and execution pattern for DB-specific examples
     *
     * This function encapsulates the common pattern for:
     * - Parsing arguments
     * - Loading configuration
     * - Getting database config
     * - Registering drivers
     * - Creating connection
     * - Running operations
     * - Cleanup
     *
     * @param argc Argument count from main()
     * @param argv Argument values from main()
     * @param dbType Database type (mysql, postgresql, sqlite, firebird)
     * @param exampleName Name for help output
     * @param operations Function to execute with the connection
     * @return Exit code (0=success, 1=error, 100=driver not enabled)
     */
    inline int runRelationalExample(
        int argc, char *argv[],
        const std::string &dbType,
        const std::string &exampleName,
        const std::function<void(std::shared_ptr<cpp_dbc::RelationalDBConnection>, const std::string &)> &operations)
    {
        log("========================================");
        log("cpp_dbc " + exampleName);
        log("========================================");
        log("");

        logStep("Parsing command line arguments...");
        auto args = parseArgs(argc, argv);

        if (args.showHelp)
        {
            printHelp(exampleName, dbType);
            return EXIT_OK_;
        }
        logOk("Arguments parsed");

        logStep("Loading configuration from: " + args.configPath);
        auto configResult = loadConfig(args.configPath);

        if (!configResult)
        {
            logError("Failed to load configuration: " + configResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!configResult.value())
        {
            logError("Configuration file not found: " + args.configPath);
            logInfo("Use --config=<path> to specify config file");
            return EXIT_ERROR_;
        }
        logOk("Configuration loaded successfully");

        auto &configManager = *configResult.value();

        logStep("Getting database configuration...");
        auto dbResult = getDbConfig(configManager, args.dbName, dbType);

        if (!dbResult)
        {
            logError("Failed to get database config: " + dbResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!dbResult.value())
        {
            logError("Database configuration not found for type: " + dbType);
            logInfo("Check your config file for a '" + dbType + "' database entry");
            return EXIT_ERROR_;
        }

        auto &dbConfig = *dbResult.value();
        logOk("Using database: " + dbConfig.getName() +
              " (" + dbConfig.getType() + "://" +
              dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
              "/" + dbConfig.getDatabase() + ")");

        logStep("Registering " + dbType + " driver...");
        if (!registerDriver(dbType))
        {
            logError("Failed to register driver for: " + dbType);
            return EXIT_ERROR_;
        }
        logOk("Driver registered");

        try
        {
            logStep("Connecting to " + dbType + "...");
            auto connBase = dbConfig.createDBConnection();
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(connBase);

            if (!conn)
            {
                logError("Failed to cast connection to RelationalDBConnection");
                return EXIT_ERROR_;
            }
            logOk("Connected to " + dbType);

            // Execute the provided operations
            operations(conn, dbType);

            log("");
            logStep("Closing connection...");
            conn->close();
            logOk("Connection closed");
        }
        catch (const cpp_dbc::DBException &e)
        {
            logError("Database error: " + e.what_s());
            return EXIT_ERROR_;
        }
        catch (const std::exception &e)
        {
            logError("Error: " + std::string(e.what()));
            return EXIT_ERROR_;
        }

        log("");
        log("========================================");
        logOk("Example completed successfully");
        log("========================================");

        return EXIT_OK_;
    }

} // namespace cpp_dbc::examples::relational

#endif // CPP_DBC_EXAMPLE_RELATIONAL_COMMON_HPP
