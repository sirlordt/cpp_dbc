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
 * @file 23_081_example_firebird_batch.cpp
 * @brief Firebird-specific example demonstrating batch operations
 *
 * This example demonstrates:
 * - Batch INSERT operations
 * - Batch UPDATE operations
 * - Batch DELETE operations
 * - Performance comparison between individual and batch operations
 * - Transaction-wrapped batch operations for atomicity
 * - Firebird EXECUTE BLOCK for batch operations
 *
 * Usage:
 *   ./23_081_example_firebird_batch [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Firebird support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_FIREBIRD
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

    conn->executeUpdate(
        "RECREATE TABLE batch_test ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "name VARCHAR(100) NOT NULL, "
        "num_value NUMERIC(10,2),"
        "category VARCHAR(50), "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")");

    logOk("Database setup completed");
}

// Demonstrate individual INSERT operations (for comparison)
void demonstrateIndividualInserts(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn, int numRecords)
{
    logMsg("");
    logMsg("--- Individual INSERT Operations ---");
    logStep("Inserting " + std::to_string(numRecords) + " records individually...");

    conn->executeUpdate("DELETE FROM batch_test");

    auto pstmt = conn->prepareStatement(
        "INSERT INTO batch_test (id, name, num_value, category) VALUES (?, ?, ?, ?)");

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
    logMsg("");
    logMsg("--- Batch INSERT Operations (Transaction) ---");
    logStep("Inserting " + std::to_string(numRecords) + " records in a transaction...");

    conn->executeUpdate("DELETE FROM batch_test");

    auto pstmt = conn->prepareStatement(
        "INSERT INTO batch_test (id, name, num_value, category) VALUES (?, ?, ?, ?)");

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
        conn->rollback();
        conn->setAutoCommit(true);
        throw;
    }

    conn->setAutoCommit(true);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Batch inserts completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");
    logData("Average: " + std::to_string(static_cast<double>(duration.count()) / numRecords) + " ms per insert");

    // Verify results
    auto rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    rs->next();
    logData("Total records inserted: " + std::to_string(rs->getInt("CNT")));
}

// Demonstrate batch UPDATE operations
void demonstrateBatchUpdates(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch UPDATE Operations ---");
    logStep("Performing batch updates within a transaction...");

    auto pstmt = conn->prepareStatement(
        "UPDATE batch_test SET num_value = num_value * ?, category = ? WHERE id = ?");

    auto startTime = std::chrono::high_resolution_clock::now();

    conn->setAutoCommit(false);

    try
    {
        // Update records in batches
        for (int i = 1; i <= 100; ++i)
        {
            pstmt->setDouble(1, 1.1); // Increase num_value by 10%
            pstmt->setString(2, "Updated_" + std::to_string(i % 3));
            pstmt->setInt(3, i);
            pstmt->executeUpdate();
        }

        conn->commit();
    }
    catch (const std::exception &e)
    {
        conn->rollback();
        conn->setAutoCommit(true);
        throw;
    }

    conn->setAutoCommit(true);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Batch updates completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");

    // Show sample of updated records
    logStep("Sample of updated records:");
    auto rs = conn->executeQuery("SELECT FIRST 5 id, name, num_value, category FROM batch_test ORDER BY id");
    printResults(rs);
}

// Demonstrate batch DELETE operations
void demonstrateBatchDeletes(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch DELETE Operations ---");

    // First show count before delete
    auto countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    logData("Records before delete: " + std::to_string(countRs->getInt("CNT")));

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
        conn->rollback();
        conn->setAutoCommit(true);
        throw;
    }

    conn->setAutoCommit(true);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("Batch deletes completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");

    // Show count after delete
    countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    logData("Records after delete: " + std::to_string(countRs->getInt("CNT")));
}

// Demonstrate EXECUTE BLOCK for batch operations (Firebird-specific)
void demonstrateExecuteBlock(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- EXECUTE BLOCK (Firebird-specific) ---");
    logStep("Using EXECUTE BLOCK for batch inserts...");

    conn->executeUpdate("DELETE FROM batch_test WHERE id > 500");

    auto startTime = std::chrono::high_resolution_clock::now();

    // Use EXECUTE BLOCK for multiple operations in a single call
    conn->executeUpdate(
        "EXECUTE BLOCK AS "
        "DECLARE VARIABLE i INTEGER; "
        "BEGIN "
        "    i = 501; "
        "    WHILE (i <= 550) DO BEGIN "
        "        INSERT INTO batch_test (id, name, num_value, category) "
        "        VALUES (:i, 'BlockItem_' || :i, :i * 1.5, 'BlockCat'); "
        "        i = i + 1; "
        "    END "
        "END");

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    logOk("EXECUTE BLOCK completed");
    logData("Time taken: " + std::to_string(duration.count()) + " ms");
    logData("Inserted 50 records using EXECUTE BLOCK");

    // Show sample
    logStep("Sample of EXECUTE BLOCK inserted records:");
    auto rs = conn->executeQuery("SELECT FIRST 5 id, name, num_value, category FROM batch_test WHERE id > 500 ORDER BY id");
    printResults(rs);
}

// Demonstrate atomic batch operations with rollback
void demonstrateAtomicBatch(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Atomic Batch Operations with Rollback ---");
    logStep("Demonstrating transaction rollback on error...");

    // Get current count
    auto countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    int countBefore = countRs->getInt("CNT");
    logData("Records before atomic batch: " + std::to_string(countBefore));

    conn->setAutoCommit(false);

    try
    {
        // Insert some records
        auto pstmt = conn->prepareStatement(
            "INSERT INTO batch_test (id, name, num_value, category) VALUES (?, ?, ?, ?)");

        for (int i = 600; i <= 605; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Atomic_" + std::to_string(i));
            pstmt->setDouble(3, i * 1.0);
            pstmt->setString(4, "AtomicCat");
            pstmt->executeUpdate();
        }

        logData("Inserted 6 records, now simulating an error...");

        // Simulate an error condition - trying to insert duplicate
        pstmt->setInt(1, 600); // Duplicate ID - will fail
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
        conn->rollback();
        conn->setAutoCommit(true);
        logOk("Transaction rolled back");
    }

    conn->setAutoCommit(true);

    // Verify rollback
    countRs = conn->executeQuery("SELECT COUNT(*) as cnt FROM batch_test");
    countRs->next();
    int countAfter = countRs->getInt("CNT");
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

    const int numRecords = 200;

    demonstrateIndividualInserts(conn, numRecords);
    demonstrateBatchInserts(conn, numRecords);
    demonstrateBatchUpdates(conn);
    demonstrateBatchDeletes(conn);
    demonstrateExecuteBlock(conn);
    demonstrateAtomicBatch(conn);

    logMsg("");
    logStep("Cleaning up...");
    conn->executeUpdate("DROP TABLE batch_test");
    logOk("Cleanup completed");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc Firebird Batch Operations Example");
    logMsg("========================================");
    logMsg("");

#if !USE_FIREBIRD
    logError("Firebird support is not enabled");
    logInfo("Build with -DUSE_FIREBIRD=ON to enable Firebird support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,firebird");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("23_081_example_firebird_batch", "firebird");
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

    logStep("Registering Firebird driver...");
    registerDriver("firebird");
    logOk("Driver registered");

    try
    {
        logStep("Getting Firebird configuration...");
        auto firebirdResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "firebird");

        if (!firebirdResult)
        {
            logError("Failed to get Firebird config: " + firebirdResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!firebirdResult.value())
        {
            logError("Firebird configuration not found");
            return EXIT_ERROR_;
        }

        auto &firebirdConfig = *firebirdResult.value();
        logOk("Using: " + firebirdConfig.getName());

        logStep("Connecting to Firebird...");
        auto firebirdConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            firebirdConfig.createDBConnection());
        logOk("Connected to Firebird");

        runAllDemonstrations(firebirdConn);

        logStep("Closing Firebird connection...");
        firebirdConn->close();
        logOk("Firebird connection closed");
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

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
