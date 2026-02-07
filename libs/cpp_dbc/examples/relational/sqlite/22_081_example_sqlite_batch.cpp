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
 * @file 22_081_example_sqlite_batch.cpp
 * @brief SQLite-specific example demonstrating batch operations
 *
 * This example demonstrates:
 * - Batch INSERT operations
 * - Batch UPDATE operations
 * - Batch DELETE operations
 * - Performance comparison between individual and batch operations
 * - Transaction-wrapped batch operations for atomicity
 *
 * Usage:
 *   ./22_081_example_sqlite_batch [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - SQLite support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_SQLITE
// Helper function to print query results
void printResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    auto columnNames = rs->getColumnNames();

    std::ostringstream headerStream;
    for (const auto &column : columnNames)
    {
        headerStream << std::setw(15) << std::left << column << " | ";
    }
    logData(headerStream.str());

    std::ostringstream sepStream;
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        sepStream << std::string(15, '-') << "-|-";
    }
    logData(sepStream.str());

    int rowCount = 0;
    while (rs->next())
    {
        std::ostringstream rowStream;
        for (const auto &column : columnNames)
        {
            std::string value = rs->isNull(column) ? "NULL" : rs->getString(column);
            rowStream << std::setw(15) << std::left << value << " | ";
        }
        logData(rowStream.str());
        rowCount++;
    }

    logData(std::to_string(rowCount) + " row(s) returned");
}

// Function to set up the test database
void setupDatabase(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logStep("Setting up test database...");

    conn->executeUpdate("DROP TABLE IF EXISTS batch_test");
    conn->executeUpdate(
        "CREATE TABLE batch_test ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL, "
        "value REAL, "
        "category TEXT, "
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")");

    logOk("Database setup completed");
}

// Demonstrate individual INSERT operations (for comparison)
void demonstrateIndividualInserts(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn, int numRecords)
{
    log("");
    log("--- Individual INSERT Operations ---");
    logStep("Inserting " + std::to_string(numRecords) + " records individually...");

    conn->executeUpdate("DELETE FROM batch_test");

    auto pstmt = conn->prepareStatement(
        "INSERT INTO batch_test (id, name, value, category) VALUES (?, ?, ?, ?)");

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 1; i <= numRecords; ++i)
    {
        pstmt->setInt(1, i);
        pstmt->setString(2, "Item_" + std::to_string(i));
        pstmt->setDouble(3, i * 1.5);
        pstmt->setString(4, "Category_" + std::to_string(i % 5));
        pstmt->executeUpdate();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Individual inserts completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");
    logData("Average: " + std::to_string(static_cast<double>(duration.count()) / numRecords) + " ms per insert");
}

// Demonstrate batch INSERT operations within a transaction
void demonstrateBatchInserts(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn, int numRecords)
{
    log("");
    log("--- Batch INSERT Operations (Transaction) ---");
    logStep("Inserting " + std::to_string(numRecords) + " records in a transaction...");

    conn->executeUpdate("DELETE FROM batch_test");

    auto pstmt = conn->prepareStatement(
        "INSERT INTO batch_test (id, name, value, category) VALUES (?, ?, ?, ?)");

    auto startTime = std::chrono::high_resolution_clock::now();

    // Start transaction for batch operation
    conn->setAutoCommit(false);

    try
    {
        for (int i = 1; i <= numRecords; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "BatchItem_" + std::to_string(i));
            pstmt->setDouble(3, i * 2.5);
            pstmt->setString(4, "BatchCat_" + std::to_string(i % 5));
            pstmt->executeUpdate();
        }

        conn->commit();
    }
    catch (const std::exception &e)
    {
        // Only rollback if a transaction is still active
        if (conn->transactionActive())
        {
            conn->rollback();
        }
        throw;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Batch inserts completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");
    logData("Average: " + std::to_string(static_cast<double>(duration.count()) / numRecords) + " ms per insert");

    // Verify results
    auto rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    rs->next();
    logData("Total records inserted: " + std::to_string(rs->getInt("cnt")));
}

// Demonstrate batch UPDATE operations
void demonstrateBatchUpdates(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Batch UPDATE Operations ---");
    logStep("Performing batch updates within a transaction...");

    auto pstmt = conn->prepareStatement(
        "UPDATE batch_test SET value = value * ?, category = ? WHERE id = ?");

    auto startTime = std::chrono::high_resolution_clock::now();

    conn->setAutoCommit(false);

    try
    {
        // Update records in batches
        for (int i = 1; i <= 100; ++i)
        {
            pstmt->setDouble(1, 1.1); // Increase value by 10%
            pstmt->setString(2, "Updated_" + std::to_string(i % 3));
            pstmt->setInt(3, i);
            pstmt->executeUpdate();
        }

        conn->commit();
    }
    catch (const std::exception &e)
    {
        // Only rollback if a transaction is still active
        if (conn->transactionActive())
        {
            conn->rollback();
        }
        throw;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Batch updates completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");

    // Show sample of updated records
    logStep("Sample of updated records:");
    auto rs = conn->executeQuery("SELECT id, name, value, category FROM batch_test WHERE id <= 5 ORDER BY id");
    printResults(rs);
}

// Demonstrate batch DELETE operations
void demonstrateBatchDeletes(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Batch DELETE Operations ---");

    // First show count before delete
    auto countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    logData("Records before delete: " + std::to_string(countRs->getInt("cnt")));

    logStep("Performing batch deletes within a transaction...");

    auto pstmt = conn->prepareStatement("DELETE FROM batch_test WHERE id = ?");

    auto startTime = std::chrono::high_resolution_clock::now();

    conn->setAutoCommit(false);

    try
    {
        // Delete even-numbered records
        for (int i = 2; i <= 100; i += 2)
        {
            pstmt->setInt(1, i);
            pstmt->executeUpdate();
        }

        conn->commit();
    }
    catch (const std::exception &e)
    {
        // Only rollback if a transaction is still active
        if (conn->transactionActive())
        {
            conn->rollback();
        }
        throw;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Batch deletes completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");

    // Show count after delete
    countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    logData("Records after delete: " + std::to_string(countRs->getInt("cnt")));
}

// Demonstrate bulk insert using multi-row VALUES
void demonstrateBulkInsert(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Bulk INSERT with Multiple VALUES ---");
    logStep("Inserting multiple rows in a single statement...");

    conn->executeUpdate("DELETE FROM batch_test WHERE id > 100");

    auto startTime = std::chrono::high_resolution_clock::now();

    // SQLite supports multi-row INSERT
    std::string sql = "INSERT INTO batch_test (id, name, value, category) VALUES ";
    for (int i = 101; i <= 150; ++i)
    {
        if (i > 101)
            sql += ", ";
        sql += "(" + std::to_string(i) + ", 'Bulk_" + std::to_string(i) +
               "', " + std::to_string(i * 3.14) +
               ", 'BulkCat_" + std::to_string(i % 3) + "')";
    }

    conn->executeUpdate(sql);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Bulk insert completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");
    logData("Inserted 50 records in a single statement");

    // Show sample
    logStep("Sample of bulk-inserted records:");
    auto rs = conn->executeQuery("SELECT id, name, value, category FROM batch_test WHERE id BETWEEN 101 AND 105 ORDER BY id");
    printResults(rs);
}

// Demonstrate atomic batch operations with rollback
void demonstrateAtomicBatch(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Atomic Batch Operations with Rollback ---");
    logStep("Demonstrating transaction rollback on error...");

    // Get current count
    auto countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    int countBefore = countRs->getInt("cnt");
    logData("Records before atomic batch: " + std::to_string(countBefore));

    conn->setAutoCommit(false);

    try
    {
        // Insert some records
        auto pstmt = conn->prepareStatement(
            "INSERT INTO batch_test (id, name, value, category) VALUES (?, ?, ?, ?)");

        for (int i = 200; i <= 205; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Atomic_" + std::to_string(i));
            pstmt->setDouble(3, i * 1.0);
            pstmt->setString(4, "AtomicCat");
            pstmt->executeUpdate();
        }

        logData("Inserted 6 records, now simulating an error...");

        // Simulate an error condition - trying to insert duplicate
        pstmt->setInt(1, 200); // Duplicate ID - will fail
        pstmt->setString(2, "Duplicate");
        pstmt->setDouble(3, 0.0);
        pstmt->setString(4, "Error");
        pstmt->executeUpdate(); // This should fail

        conn->commit();
    }
    catch (const cpp_dbc::DBException &e)
    {
        logData("Error occurred (as expected): " + e.what_s());
        logStep("Rolling back transaction...");
        // Only rollback if a transaction is still active
        if (conn->transactionActive())
        {
            conn->rollback();
            logOk("Transaction rolled back");
        }
        else
        {
            logOk("Transaction already rolled back automatically");
        }
    }

    // Verify rollback
    countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    int countAfter = countRs->getInt("cnt");
    logData("Records after rollback: " + std::to_string(countAfter));

    if (countBefore == countAfter)
    {
        logOk("Atomicity verified - no partial inserts");
    }
    else
    {
        logError("Atomicity failed - partial inserts detected");
    }
}

void runAllDemonstrations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    setupDatabase(conn);

    const int numRecords = 500;

    demonstrateIndividualInserts(conn, numRecords);
    demonstrateBatchInserts(conn, numRecords);
    demonstrateBatchUpdates(conn);
    demonstrateBatchDeletes(conn);
    demonstrateBulkInsert(conn);
    demonstrateAtomicBatch(conn);

    log("");
    logStep("Cleaning up...");
    conn->executeUpdate("DROP TABLE IF EXISTS batch_test");
    logOk("Cleanup completed");
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc SQLite Batch Operations Example");
    log("========================================");
    log("");

#if !USE_SQLITE
    logError("SQLite support is not enabled");
    logInfo("Build with -DUSE_SQLITE=ON to enable SQLite support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,sqlite");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("22_081_example_sqlite_batch", "sqlite");
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

    logStep("Registering SQLite driver...");
    registerDriver("sqlite");
    logOk("Driver registered");

    try
    {
        logStep("Getting SQLite configuration...");
        auto sqliteResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "sqlite");

        if (!sqliteResult)
        {
            logError("Failed to get SQLite config: " + sqliteResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!sqliteResult.value())
        {
            logError("SQLite configuration not found");
            return EXIT_ERROR_;
        }

        auto &sqliteConfig = *sqliteResult.value();
        logOk("Using: " + sqliteConfig.getName());

        logStep("Connecting to SQLite...");
        auto sqliteConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            sqliteConfig.createDBConnection());
        logOk("Connected to SQLite");

        runAllDemonstrations(sqliteConn);

        logStep("Closing SQLite connection...");
        sqliteConn->close();
        logOk("SQLite connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        e.printCallStack();
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
#endif
}
