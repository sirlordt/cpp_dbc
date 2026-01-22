/*

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

 @file transaction_manager.cpp
 @brief Tests for transaction management

*/

#include "cpp_dbc/transaction_manager.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

// Debug output is controlled by -DDEBUG_TRANSACTION_MANAGER=1 CMake option
#if (defined(DEBUG_TRANSACTION_MANAGER) && DEBUG_TRANSACTION_MANAGER) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define TM_DEBUG(x) std::cout << x << std::endl
#else
#define TM_DEBUG(x)
#endif

namespace cpp_dbc
{

    TransactionManager::TransactionManager(RelationalDBConnectionPool &connectionPool)
        : pool(connectionPool),
          cleanupThread(&TransactionManager::cleanupTask, this)
    {
    }

    TransactionManager::~TransactionManager()
    {
        TM_DEBUG("TransactionManager::~TransactionManager - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        try
        {
            close();
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            TM_DEBUG("TransactionManager::~TransactionManager - Exception in destructor: " << ex.what());
        }

        TM_DEBUG("TransactionManager::~TransactionManager - Destructor completed at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    std::string TransactionManager::beginTransaction()
    {
        // Get a connection from the pool
        std::shared_ptr<RelationalDBConnection> conn = pool.getRelationalDBConnection();

        // Start transaction using the connection's method
        conn->beginTransaction();

        // Generate a unique transaction ID
        std::string transactionId = generateUUID();

        // Create and store transaction context
        auto transContext = std::make_shared<TransactionContext>(conn, transactionId);

        // Lock and store in the active transactions map
        {
            std::scoped_lock lockTrans(transactionMutex);
            activeTransactions[transactionId] = transContext;
        }

        return transactionId;
    }

    std::shared_ptr<RelationalDBConnection> TransactionManager::getTransactionDBConnection(const std::string &transactionId)
    {
        std::scoped_lock lockTrans(transactionMutex);

        auto it = activeTransactions.find(transactionId);
        if (it == activeTransactions.end())
        {
            throw DBException("3A4B5C6D7E8F", "Transaction not found: " + transactionId, system_utils::captureCallStack());
        }

        // Update last access time
        it->second->lastAccessTime = std::chrono::steady_clock::now();

        return it->second->connection;
    }

    void TransactionManager::commitTransaction(const std::string &transactionId)
    {
        std::shared_ptr<TransactionContext> transContext;

        // Find and remove the transaction from the active map
        {
            std::scoped_lock lockTrans(transactionMutex);

            auto it = activeTransactions.find(transactionId);
            if (it == activeTransactions.end())
            {
                throw DBException("9G0H1I2J3K4L", "Transaction not found: " + transactionId, system_utils::captureCallStack());
            }

            transContext = it->second;
            activeTransactions.erase(it);
        }

        // Commit the transaction
        try
        {
            transContext->connection->commit();

            // Return the connection to the pool
            // No need to call close() first, as returnToPool() will handle the connection properly
            transContext->connection->returnToPool();
        }
        catch ([[maybe_unused]] const DBException &ex)
        {
            // Ensure connection is returned to pool even on error
            try
            {
                // Ensure the connection is properly returned to the pool
                if (transContext->connection->isPooled())
                {
                    transContext->connection->returnToPool();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex2)
            {
                TM_DEBUG("TransactionManager::commitTransaction - Exception returning to pool: " << ex2.what());
            }
            throw; // Re-throw the original exception
        }
    }

    void TransactionManager::rollbackTransaction(const std::string &transactionId)
    {
        std::shared_ptr<TransactionContext> transContext;

        // Find and remove the transaction from the active map
        {
            std::scoped_lock lockTrans(transactionMutex);

            auto it = activeTransactions.find(transactionId);
            if (it == activeTransactions.end())
            {
                throw DBException("5M6N7O8P9Q0R", "Transaction not found: " + transactionId, system_utils::captureCallStack());
            }

            transContext = it->second;
            activeTransactions.erase(it);
        }

        // Rollback the transaction
        try
        {
            transContext->connection->rollback();

            // Return the connection to the pool
            // No need to call close() first, as returnToPool() will handle the connection properly
            transContext->connection->returnToPool();
        }
        catch ([[maybe_unused]] const DBException &ex)
        {
            // Ensure connection is returned to pool even on error
            try
            {
                // Ensure the connection is properly returned to the pool
                if (transContext->connection->isPooled())
                {
                    transContext->connection->returnToPool();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex2)
            {
                TM_DEBUG("TransactionManager::rollbackTransaction - Exception returning to pool: " << ex2.what());
            }
            throw; // Re-throw the original exception
        }
    }

    bool TransactionManager::isTransactionActive(const std::string &transactionId)
    {
        std::string expiredTransactionId;

        // First, check if the transaction exists and if it has timed out
        {
            std::scoped_lock lockTrans(transactionMutex);

            auto it = activeTransactions.find(transactionId);
            if (it == activeTransactions.end())
            {
                return false;
            }

            // Check if the transaction has timed out
            auto now = std::chrono::steady_clock::now();
            auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - it->second->lastAccessTime)
                                .count();

            if (idleTime > transactionTimeoutMillis)
            {
                // Transaction has timed out, we need to clean it up
                expiredTransactionId = transactionId;
            }
        }

        // If the transaction has expired, clean it up outside the lock
        if (!expiredTransactionId.empty())
        {
            try
            {
                rollbackTransaction(expiredTransactionId);
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                TM_DEBUG("TransactionManager::isTransactionActive - Exception during rollback: " << ex.what());
            }
            return false;
        }

        return true;
    }

    size_t TransactionManager::getActiveTransactionCount()
    {
        std::scoped_lock lockTrans(transactionMutex);
        return activeTransactions.size();
    }

    void TransactionManager::setTransactionTimeout(long timeoutMillis)
    {
        this->transactionTimeoutMillis = timeoutMillis;
    }

    void TransactionManager::cleanupTask()
    {
        while (running)
        {
            // Wait for the cleanup interval or until notified to stop
            std::unique_lock lockCleanup(cleanupMutex);  // NOSONAR - unique_lock required for condition_variable
            cleanupCondition.wait_for(lockCleanup, std::chrono::milliseconds(cleanupIntervalMillis));

            if (!running)
            {
                break;
            }

            std::vector<std::string> expiredTransactions;

            // Find expired transactions
            {
                std::scoped_lock lockTrans(transactionMutex);

                auto now = std::chrono::steady_clock::now();

                for (const auto &[transId, transContext] : activeTransactions)
                {
                    auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - transContext->lastAccessTime)
                                        .count();

                    if (idleTime > transactionTimeoutMillis)
                    {
                        expiredTransactions.push_back(transId);
                    }
                }
            }

            // Rollback expired transactions
            for (const auto &transId : expiredTransactions)
            {
                try
                {
                    rollbackTransaction(transId);
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    TM_DEBUG("TransactionManager::cleanupTask - Exception during rollback: " << ex.what());
                }
            }
        }
    }

    std::string TransactionManager::generateUUID() const
    {
        // Simple UUID generation using random numbers
        // In a production environment, consider using a proper UUID library
        // Using thread_local to ensure thread-safety when called concurrently

        thread_local std::random_device rd;
        thread_local std::mt19937 gen(rd());
        thread_local std::uniform_int_distribution dis(0, 15);
        thread_local std::uniform_int_distribution dis2(8, 11);

        std::stringstream ss;
        ss << std::hex;

        for (int i = 0; i < 8; i++)
        {
            ss << dis(gen);
        }
        ss << "-";

        for (int i = 0; i < 4; i++)
        {
            ss << dis(gen);
        }
        ss << "-4"; // Version 4 UUID

        for (int i = 0; i < 3; i++)
        {
            ss << dis(gen);
        }
        ss << "-";

        ss << dis2(gen);
        for (int i = 0; i < 3; i++)
        {
            ss << dis(gen);
        }
        ss << "-";

        for (int i = 0; i < 12; i++)
        {
            ss << dis(gen);
        }

        return ss.str();
    }

    void TransactionManager::close()
    {
        TM_DEBUG("TransactionManager::close - Starting close operation");

        if (!running.exchange(false))
        {
            TM_DEBUG("TransactionManager::close - Already closed, returning");
            return; // Already closed
        }

        TM_DEBUG("TransactionManager::close - Setting running flag to false at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Notify the cleanup thread to wake up and exit
        TM_DEBUG("TransactionManager::close - Notifying cleanup thread at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        cleanupCondition.notify_all();

        // Join the cleanup thread
        if (cleanupThread.joinable())
        {
            TM_DEBUG("TransactionManager::close - Starting to join cleanup thread at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            cleanupThread.join();
            TM_DEBUG("TransactionManager::close - Cleanup thread joined at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        }
        else
        {
            TM_DEBUG("TransactionManager::close - Cleanup thread not joinable at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        }

        // Rollback all active transactions
        std::vector<std::string> transactionIds;

        {
            TM_DEBUG("TransactionManager::close - Acquiring transaction mutex at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            std::scoped_lock lockTrans(transactionMutex);
            TM_DEBUG("TransactionManager::close - Acquired transaction mutex at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            TM_DEBUG("TransactionManager::close - Found " << activeTransactions.size() << " active transactions at "
                                                          << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            for (const auto &[transId, transContext] : activeTransactions)
            {
                transactionIds.push_back(transId);
                static_cast<void>(transContext); // Silence unused variable warning
            }
        }
        TM_DEBUG("TransactionManager::close - Collected transaction IDs at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        for (size_t i = 0; i < transactionIds.size(); i++)
        {
            const auto &transId = transactionIds[i];
            TM_DEBUG("TransactionManager::close - Rolling back transaction " << (i + 1) << "/" << transactionIds.size()
                                                                             << " (ID: " << transId << ") at "
                                                                             << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            try
            {
                rollbackTransaction(transId);
                TM_DEBUG("TransactionManager::close - Successfully rolled back transaction " << transId << " at "
                                                                                             << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            }
            catch (const std::exception &ex)
            {
                TM_DEBUG("TransactionManager::close - Exception rolling back transaction " << transId << ": " << ex.what());
            }
        }
        TM_DEBUG("TransactionManager::close - Rolled back all transactions at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Clear the transactions map
        {
            TM_DEBUG("TransactionManager::close - Acquiring transaction mutex to clear map at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            std::scoped_lock lockTrans(transactionMutex);
            TM_DEBUG("TransactionManager::close - Acquired transaction mutex at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            activeTransactions.clear();
            TM_DEBUG("TransactionManager::close - Cleared transactions map at "
                     << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        }

        TM_DEBUG("TransactionManager::close - Close operation completed");
    }

} // namespace cpp_dbc
