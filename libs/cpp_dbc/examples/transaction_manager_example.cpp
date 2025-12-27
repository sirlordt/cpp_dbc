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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file transaction_manager_example.cpp
 @brief Tests for transaction management

*/
// CPPDBC_TransactionManager_Example.cpp
// Example of using the CPPDBC Transaction Manager across threads

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include "cpp_dbc/connection_pool.hpp"
#include "cpp_dbc/transaction_manager.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

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
                std::cout << "Worker " << workerId << " processing task " << task.taskId
                          << " for transaction " << task.transactionId << std::endl;
            }

            // Execute the task
            task.task();

            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Worker " << workerId << " completed task " << task.taskId << std::endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Worker " << workerId << " error: " << e.what() << std::endl;
    }
}

int main()
{
    try
    {
#if USE_MYSQL
        // Initialize MySQL driver and connection pool
        cpp_dbc::config::DBConnectionPoolConfig config;
        config.setUrl("cpp_dbc:mysql://localhost:3306/testdb");
        config.setUsername("username");
        config.setPassword("password");
        config.setInitialSize(5);
        config.setMaxSize(20);

        cpp_dbc::MySQL::MySQLConnectionPool pool(config);

        // Create transaction manager
        cpp_dbc::TransactionManager txnManager(pool);
        txnManager.setTransactionTimeout(60000); // 1 minute timeout

        // Create task queue and workers
        TaskQueue taskQueue;
        const int numWorkers = 4;
        std::vector<std::thread> workers;

        for (int i = 0; i < numWorkers; i++)
        {
            workers.push_back(std::thread(workerThread, std::ref(txnManager), std::ref(taskQueue), i));
        }

        // Simulate multiple business processes with transactions
        const int numTransactions = 5;
        std::vector<std::string> transactionIds;

        // Start transactions and create initial tasks
        for (int i = 0; i < numTransactions; i++)
        {
            std::string txnId = txnManager.beginTransaction();
            transactionIds.push_back(txnId);

            std::cout << "Started transaction " << txnId << std::endl;

            // Create first task for this transaction
            taskQueue.push(WorkflowTask(txnId, 1, [&txnManager, txnId]()
                                        {
                try {
                    auto conn = txnManager.getTransactionDBConnection(txnId);
                    
                    // Perform some database operations in this transaction
                    conn->executeUpdate("INSERT INTO transaction_test (id, data) VALUES (1, 'Task 1 Data')");
                    
                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 + rand() % 200));
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cerr << "Error in task 1: " << e.what() << std::endl;
                    throw; // Re-throw to properly handle the error
                } }));
        }

        // Add second tasks for each transaction
        for (const auto &txnId : transactionIds)
        {
            taskQueue.push(WorkflowTask(txnId, 2, [&txnManager, txnId]()
                                        {
                try {
                    auto conn = txnManager.getTransactionDBConnection(txnId);
                    
                    // Perform more database operations in this transaction
                    conn->executeUpdate("UPDATE transaction_test SET data = 'Task 2 Updated' WHERE id = 1");
                    
                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::milliseconds(150 + rand() % 250));
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cerr << "Error in task 2: " << e.what() << std::endl;
                    throw;
                } }));
        }

        // Add final tasks to commit or rollback transactions
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
                        std::cout << "Committing transaction " << txnId << std::endl;
                        txnManager.commitTransaction(txnId);
                    } else {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        std::cout << "Rolling back transaction " << txnId << std::endl;
                        txnManager.rollbackTransaction(txnId);
                    }
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cerr << "Error in commit/rollback: " << e.what() << std::endl;
                } }));
        }

        // Signal that we're done adding tasks
        taskQueue.finish();

        // Wait for all workers to finish
        for (auto &worker : workers)
        {
            worker.join();
        }

        std::cout << "All workers completed." << std::endl;
        std::cout << "Remaining active transactions: " << txnManager.getActiveTransactionCount() << std::endl;

        // Cleanup
        txnManager.close();
        pool.close();

        std::cout << "Transaction manager and connection pool closed." << std::endl;
#else
        std::cout << "MySQL support is not enabled. This example requires MySQL." << std::endl;
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
