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
 * @file 22_041_example_sqlite_transaction_manager.cpp
 * @brief SQLite-specific example demonstrating transaction management
 *
 * This example demonstrates:
 * - Transaction management with SQLite connection pools
 * - Multi-threaded workflow processing
 * - Transaction commit and rollback
 *
 * Note: SQLite has specific transaction semantics. This example uses
 * WAL mode and careful serialization for thread safety.
 *
 * Usage:
 *   ./22_041_example_sqlite_transaction_manager [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - SQLite support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include "cpp_dbc/core/relational/relational_db_connection_pool.hpp"
#include "cpp_dbc/transaction_manager.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_SQLITE
// Mutex for thread-safe console output
std::mutex consoleMutex;

// Structure to represent a task in a workflow
struct WorkflowTask
{
    std::string transactionId;
    int taskId;
    std::function<void()> task;

    WorkflowTask(std::string txnId, int id, std::function<void()> fn)
        : transactionId(std::move(txnId)), taskId(id), task(std::move(fn)) {}
};

// Thread-safe task queue
class TaskQueue
{
private:
    std::queue<WorkflowTask> tasks;
    std::mutex mutex;
    std::condition_variable condition;
    bool done;

public:
    TaskQueue() : done(false) {}

    void push(WorkflowTask task)
    {
        std::lock_guard<std::mutex> lock(mutex);
        tasks.push(std::move(task));
        condition.notify_one();
    }

    bool pop(WorkflowTask &task)
    {
        std::unique_lock<std::mutex> lock(mutex);

        condition.wait(lock, [this]
                       { return !tasks.empty() || done; });

        if (done && tasks.empty())
        {
            return false;
        }

        task = std::move(tasks.front());
        tasks.pop();
        return true;
    }

    void finish()
    {
        std::lock_guard<std::mutex> lock(mutex);
        done = true;
        condition.notify_all();
    }
};

// Worker thread function
void workerThread(cpp_dbc::TransactionManager & /*txnManager*/, TaskQueue &taskQueue, int workerId)
{
    try
    {
        WorkflowTask task{"", 0, [] {}}; // Initialize with default values

        while (taskQueue.pop(task))
        {
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                logData("Worker " + std::to_string(workerId) + " processing task " + std::to_string(task.taskId) +
                        " for transaction " + task.transactionId);
            }

            // Execute the task
            task.task();

            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                logOk("Worker " + std::to_string(workerId) + " completed task " + std::to_string(task.taskId));
            }
        }
    }
    catch (const std::exception &e)
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        logError("Worker " + std::to_string(workerId) + " error: " + std::string(e.what()));
    }
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc SQLite Transaction Manager Example");
    logMsg("========================================");
    logMsg("");

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
        printHelp("22_041_example_sqlite_transaction_manager", "sqlite");
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

    try
    {
        logStep("Registering SQLite driver...");
        registerDriver("sqlite");
        logOk("SQLite driver registered");

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

        // Create connection pool configuration
        logStep("Creating connection pool configuration...");
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(sqliteConfig.createConnectionString());
        poolConfig.setUsername(sqliteConfig.getUsername());
        poolConfig.setPassword(sqliteConfig.getPassword());
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        logOk("Pool configuration created");

        logStep("Creating connection pool...");
        auto pool = cpp_dbc::SQLite::SQLiteConnectionPool::create(poolConfig);
        logOk("Connection pool created");

        // Enable WAL mode for better concurrent access
        logStep("Enabling WAL mode for better concurrency...");
        {
            auto conn = pool->getRelationalDBConnection();
            conn->executeUpdate("PRAGMA journal_mode=WAL");
        }
        logOk("WAL mode enabled");

        // Create transaction manager
        logStep("Creating transaction manager...");
        cpp_dbc::TransactionManager txnManager(*pool);
        txnManager.setTransactionTimeout(60000); // 1 minute timeout
        logOk("Transaction manager created with 60s timeout");

        // Create task queue and workers
        logStep("Creating task queue and workers...");
        TaskQueue taskQueue;
        const int numWorkers = 2; // SQLite works better with fewer concurrent writers
        std::vector<std::thread> workers;

        for (int i = 0; i < numWorkers; i++)
        {
            workers.push_back(std::thread(workerThread, std::ref(txnManager), std::ref(taskQueue), i));
        }
        logOk("Created " + std::to_string(numWorkers) + " worker threads");
        logInfo("Note: Using fewer workers due to SQLite's write serialization");

        // Simulate multiple business processes with transactions
        logMsg("");
        logMsg("--- Starting Transactions ---");

        const int numTransactions = 3;
        std::vector<std::string> transactionIds;

        // Start transactions and create initial tasks
        for (int i = 0; i < numTransactions; i++)
        {
            std::string txnId = txnManager.beginTransaction();
            transactionIds.push_back(txnId);

            logData("Started transaction " + txnId);

            // Create first task for this transaction
            taskQueue.push(WorkflowTask(txnId, 1, [&txnManager, txnId]()
                                        {
                try {
                    auto conn = txnManager.getTransactionDBConnection(txnId);

                    // Perform some database operations in this transaction
                    conn->executeUpdate("INSERT INTO transaction_test (id, data) VALUES (1, 'Task 1 Data')");

                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rand() % 100));
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    logError("Error in task 1: " + std::string(e.what()));
                    throw;
                } }));
        }

        // Add second tasks for each transaction
        logMsg("");
        logMsg("--- Adding Update Tasks ---");

        for (const auto &txnId : transactionIds)
        {
            taskQueue.push(WorkflowTask(txnId, 2, [&txnManager, txnId]()
                                        {
                try {
                    auto conn = txnManager.getTransactionDBConnection(txnId);

                    // Perform more database operations in this transaction
                    conn->executeUpdate("UPDATE transaction_test SET data = 'Task 2 Updated' WHERE id = 1");

                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rand() % 100));
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    logError("Error in task 2: " + std::string(e.what()));
                    throw;
                } }));
        }

        // Add final tasks to commit or rollback transactions
        logMsg("");
        logMsg("--- Adding Commit/Rollback Tasks ---");

        for (size_t i = 0; i < transactionIds.size(); i++)
        {
            const std::string &txnId = transactionIds[i];

            // For demonstration, randomly commit or rollback transactions
            bool shouldCommit = (i % 3 != 0); // Commit 2/3 of transactions

            taskQueue.push(WorkflowTask(txnId, 3, [&txnManager, txnId, shouldCommit]()
                                        {
                try {
                    if (shouldCommit) {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        logStep("Committing transaction " + txnId);
                        txnManager.commitTransaction(txnId);
                        logOk("Transaction " + txnId + " committed");
                    } else {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        logStep("Rolling back transaction " + txnId);
                        txnManager.rollbackTransaction(txnId);
                        logOk("Transaction " + txnId + " rolled back");
                    }
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    logError("Error in commit/rollback: " + std::string(e.what()));
                } }));
        }

        // Signal that we're done adding tasks
        logMsg("");
        logStep("Finishing task queue...");
        taskQueue.finish();

        // Wait for all workers to finish
        logStep("Waiting for workers to complete...");
        for (auto &worker : workers)
        {
            worker.join();
        }
        logOk("All workers completed");

        logData("Remaining active transactions: " + std::to_string(txnManager.getActiveTransactionCount()));

        // Cleanup
        logStep("Closing transaction manager...");
        txnManager.close();
        logOk("Transaction manager closed");

        logStep("Closing connection pool...");
        pool->close();
        logOk("Connection pool closed");
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
