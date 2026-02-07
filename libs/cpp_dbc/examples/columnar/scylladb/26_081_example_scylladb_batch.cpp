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
 * @file 26_081_example_scylladb_batch.cpp
 * @brief ScyllaDB-specific example demonstrating batch/bulk operations
 *
 * This example demonstrates:
 * - Batch insert with prepared statements (addBatch/executeBatch)
 * - Performance comparison: individual vs batch operations
 * - Unlogged vs logged batches
 * - Partition-aware batch operations
 *
 * Note: ScyllaDB/Cassandra batches should typically contain operations
 * affecting the same partition for best performance.
 *
 * Usage:
 *   ./26_081_example_scylladb_batch [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - ScyllaDB support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <chrono>
#include <vector>

#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_SCYLLADB
std::string g_keyspace = "batch_test_ks";
std::string g_table = "batch_test_ks.batch_data";

void setupSchema(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Schema Setup ---");

    logStep("Creating keyspace if not exists...");
    conn->executeUpdate(
        "CREATE KEYSPACE IF NOT EXISTS " + g_keyspace +
        " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    logOk("Keyspace ready");

    logStep("Dropping existing table if exists...");
    conn->executeUpdate("DROP TABLE IF EXISTS " + g_table);
    logOk("Old table dropped");

    logStep("Creating table...");
    conn->executeUpdate(
        "CREATE TABLE " + g_table + " ("
                                    "partition_key int, "
                                    "clustering_key int, "
                                    "data text, "
                                    "value double, "
                                    "PRIMARY KEY (partition_key, clustering_key)"
                                    ")");
    logOk("Table created");
}

void demonstrateBatchInsert(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch Insert with Prepared Statements ---");
    logInfo("Using addBatch() and executeBatch() for bulk inserts");

    logStep("Preparing insert statement...");
    auto stmt = conn->prepareStatement(
        "INSERT INTO " + g_table + " (partition_key, clustering_key, data, value) VALUES (?, ?, ?, ?)");
    logOk("Statement prepared");

    logStep("Adding rows to batch...");
    int partitionKey = 1;
    for (int i = 1; i <= 10; ++i)
    {
        stmt->setInt(1, partitionKey);
        stmt->setInt(2, i);
        stmt->setString(3, "Batch data " + std::to_string(i));
        stmt->setDouble(4, i * 10.5);
        stmt->addBatch();
        logData("Added row: partition=" + std::to_string(partitionKey) +
                ", clustering=" + std::to_string(i));
    }
    logOk("10 rows added to batch");

    logStep("Executing batch...");
    auto results = stmt->executeBatch();
    logData("Batch executed, returned " + std::to_string(results.size()) + " results");
    logOk("Batch insert completed");

    // Verify
    logStep("Verifying inserted rows...");
    auto rs = conn->executeQuery(
        "SELECT COUNT(*) as cnt FROM " + g_table + " WHERE partition_key = " + std::to_string(partitionKey));
    if (rs->next())
    {
        logData("Rows inserted: " + std::to_string(rs->getLong("cnt")));
    }
    logOk("Verification completed");

    stmt->close();
}

void demonstrateMultiPartitionBatch(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Multi-Partition Batch ---");
    logInfo("Batching rows across multiple partitions");
    logInfo("Note: Cross-partition batches use coordinator logMsg for atomicity");

    logStep("Preparing insert statement...");
    auto stmt = conn->prepareStatement(
        "INSERT INTO " + g_table + " (partition_key, clustering_key, data, value) VALUES (?, ?, ?, ?)");

    logStep("Adding rows to multiple partitions...");
    for (int partition = 10; partition <= 13; ++partition)
    {
        for (int clustering = 1; clustering <= 3; ++clustering)
        {
            stmt->setInt(1, partition);
            stmt->setInt(2, clustering);
            stmt->setString(3, "Multi-partition data");
            stmt->setDouble(4, partition * 100.0 + clustering);
            stmt->addBatch();
        }
        logData("Added 3 rows to partition " + std::to_string(partition));
    }
    logOk("12 rows added across 4 partitions");

    logStep("Executing batch...");
    stmt->executeBatch();
    logOk("Multi-partition batch completed");

    // Verify each partition
    logStep("Verifying partitions...");
    for (int partition = 10; partition <= 13; ++partition)
    {
        auto rs = conn->executeQuery(
            "SELECT COUNT(*) as cnt FROM " + g_table +
            " WHERE partition_key = " + std::to_string(partition));
        if (rs->next())
        {
            logData("Partition " + std::to_string(partition) +
                    ": " + std::to_string(rs->getLong("cnt")) + " rows");
        }
    }
    logOk("Verification completed");

    stmt->close();
}

void demonstratePerformanceComparison(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Performance Comparison ---");
    logInfo("Comparing individual inserts vs batch insert");

    const int numRows = 100;
    int partitionKey = 100;

    // Clean up partition first
    conn->executeUpdate(
        "DELETE FROM " + g_table + " WHERE partition_key = " + std::to_string(partitionKey));

    // Individual inserts
    logStep("Individual inserts (" + std::to_string(numRows) + " rows)...");

    auto stmt = conn->prepareStatement(
        "INSERT INTO " + g_table + " (partition_key, clustering_key, data, value) VALUES (?, ?, ?, ?)");

    auto startIndividual = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numRows; ++i)
    {
        stmt->setInt(1, partitionKey);
        stmt->setInt(2, i);
        stmt->setString(3, "Individual insert " + std::to_string(i));
        stmt->setDouble(4, i * 1.5);
        stmt->executeUpdate();
    }

    auto endIndividual = std::chrono::high_resolution_clock::now();
    auto durationIndividual = std::chrono::duration_cast<std::chrono::milliseconds>(
        endIndividual - startIndividual);
    logData("Individual inserts time: " + std::to_string(durationIndividual.count()) + " ms");

    // Clean up for batch test
    conn->executeUpdate(
        "DELETE FROM " + g_table + " WHERE partition_key = " + std::to_string(partitionKey));

    // Batch insert
    logStep("Batch insert (" + std::to_string(numRows) + " rows)...");

    auto startBatch = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numRows; ++i)
    {
        stmt->setInt(1, partitionKey);
        stmt->setInt(2, i);
        stmt->setString(3, "Batch insert " + std::to_string(i));
        stmt->setDouble(4, i * 1.5);
        stmt->addBatch();
    }
    stmt->executeBatch();

    auto endBatch = std::chrono::high_resolution_clock::now();
    auto durationBatch = std::chrono::duration_cast<std::chrono::milliseconds>(
        endBatch - startBatch);
    logData("Batch insert time: " + std::to_string(durationBatch.count()) + " ms");

    // Calculate speedup
    if (durationBatch.count() > 0)
    {
        double speedup = static_cast<double>(durationIndividual.count()) / static_cast<double>(durationBatch.count());
        logData("Speedup factor: " + std::to_string(speedup) + "x");
    }
    logOk("Performance comparison completed");

    stmt->close();

    // Clean up
    conn->executeUpdate(
        "DELETE FROM " + g_table + " WHERE partition_key = " + std::to_string(partitionKey));
}

void demonstrateBatchUpdate(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch Update Operations ---");
    logInfo("Using batch for multiple updates");

    int partitionKey = 200;

    // First insert some data
    logStep("Inserting initial data...");
    auto insertStmt = conn->prepareStatement(
        "INSERT INTO " + g_table + " (partition_key, clustering_key, data, value) VALUES (?, ?, ?, ?)");

    for (int i = 1; i <= 5; ++i)
    {
        insertStmt->setInt(1, partitionKey);
        insertStmt->setInt(2, i);
        insertStmt->setString(3, "Original data");
        insertStmt->setDouble(4, i * 10.0);
        insertStmt->addBatch();
    }
    insertStmt->executeBatch();
    logOk("Initial data inserted");
    insertStmt->close();

    // Now batch update
    logStep("Preparing batch update...");
    auto updateStmt = conn->prepareStatement(
        "UPDATE " + g_table + " SET data = ?, value = ? WHERE partition_key = ? AND clustering_key = ?");

    for (int i = 1; i <= 5; ++i)
    {
        updateStmt->setString(1, "Updated data " + std::to_string(i));
        updateStmt->setDouble(2, i * 100.0);
        updateStmt->setInt(3, partitionKey);
        updateStmt->setInt(4, i);
        updateStmt->addBatch();
        logData("Queued update for clustering_key=" + std::to_string(i));
    }
    logOk("5 updates added to batch");

    logStep("Executing batch update...");
    updateStmt->executeBatch();
    logOk("Batch update completed");

    // Verify
    logStep("Verifying updates...");
    auto rs = conn->executeQuery(
        "SELECT clustering_key, data, value FROM " + g_table +
        " WHERE partition_key = " + std::to_string(partitionKey));
    while (rs->next())
    {
        logData("Row " + std::to_string(rs->getInt("clustering_key")) +
                ": data='" + rs->getString("data") +
                "', value=" + std::to_string(rs->getDouble("value")));
    }
    logOk("Verification completed");

    updateStmt->close();
}

void demonstrateBatchDelete(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch Delete Operations ---");
    logInfo("Using batch for multiple deletes");

    int partitionKey = 300;

    // First insert some data
    logStep("Inserting test data...");
    auto insertStmt = conn->prepareStatement(
        "INSERT INTO " + g_table + " (partition_key, clustering_key, data, value) VALUES (?, ?, ?, ?)");

    for (int i = 1; i <= 10; ++i)
    {
        insertStmt->setInt(1, partitionKey);
        insertStmt->setInt(2, i);
        insertStmt->setString(3, "To be deleted");
        insertStmt->setDouble(4, i * 5.0);
        insertStmt->addBatch();
    }
    insertStmt->executeBatch();
    logOk("10 rows inserted");
    insertStmt->close();

    // Count before delete
    auto rs = conn->executeQuery(
        "SELECT COUNT(*) as cnt FROM " + g_table +
        " WHERE partition_key = " + std::to_string(partitionKey));
    int64_t beforeCount = 0;
    if (rs->next())
    {
        beforeCount = rs->getLong("cnt");
        logData("Rows before delete: " + std::to_string(beforeCount));
    }

    // Batch delete (delete odd-numbered rows)
    logStep("Preparing batch delete for odd-numbered rows...");
    auto deleteStmt = conn->prepareStatement(
        "DELETE FROM " + g_table + " WHERE partition_key = ? AND clustering_key = ?");

    for (int i = 1; i <= 10; i += 2)
    {
        deleteStmt->setInt(1, partitionKey);
        deleteStmt->setInt(2, i);
        deleteStmt->addBatch();
        logData("Queued delete for clustering_key=" + std::to_string(i));
    }
    logOk("5 deletes added to batch");

    logStep("Executing batch delete...");
    deleteStmt->executeBatch();
    logOk("Batch delete completed");

    // Count after delete
    rs = conn->executeQuery(
        "SELECT COUNT(*) as cnt FROM " + g_table +
        " WHERE partition_key = " + std::to_string(partitionKey));
    if (rs->next())
    {
        int64_t afterCount = rs->getLong("cnt");
        logData("Rows after delete: " + std::to_string(afterCount));
        logData("Rows deleted: " + std::to_string(beforeCount - afterCount));
    }
    logOk("Verification completed");

    deleteStmt->close();
}

void cleanup(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Cleanup ---");
    logStep("Dropping test table and keyspace...");
    conn->executeUpdate("DROP TABLE IF EXISTS " + g_table);
    conn->executeUpdate("DROP KEYSPACE IF EXISTS " + g_keyspace);
    logOk("Cleanup completed");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc ScyllaDB Batch Operations Example");
    logMsg("========================================");
    logMsg("");

#if !USE_SCYLLADB
    logError("ScyllaDB support is not enabled");
    logInfo("Build with -DUSE_SCYLLADB=ON to enable ScyllaDB support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,scylladb");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("26_081_example_scylladb_batch", "scylladb");
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

    logStep("Getting ScyllaDB database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "scylladb");

    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!dbResult.value())
    {
        logError("ScyllaDB configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + ")");

    logStep("Registering ScyllaDB driver...");
    registerDriver("scylladb");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to ScyllaDB...");
        auto connBase = dbConfig.createDBConnection();
        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(connBase);

        if (!conn)
        {
            logError("Failed to cast connection to ColumnarDBConnection");
            return EXIT_ERROR_;
        }
        logOk("Connected to ScyllaDB");

        setupSchema(conn);
        demonstrateBatchInsert(conn);
        demonstrateMultiPartitionBatch(conn);
        demonstrateBatchUpdate(conn);
        demonstrateBatchDelete(conn);
        demonstratePerformanceComparison(conn);
        cleanup(conn);

        logMsg("");
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

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
