// CPPDBC_TransactionManager.cpp
// Implementation of transaction manager

#include "cpp_dbc/transaction_manager.hpp"
#include <sstream>
#include <iomanip>

namespace cpp_dbc
{

    TransactionManager::TransactionManager(ConnectionPool &connectionPool)
        : pool(connectionPool), running(true)
    {

        // Start the cleanup thread
        cleanupThread = std::thread(&TransactionManager::cleanupTask, this);
    }

    TransactionManager::~TransactionManager()
    {
        close();
    }

    std::string TransactionManager::beginTransaction()
    {
        // Get a connection from the pool
        std::shared_ptr<Connection> conn = pool.getConnection();

        // Disable auto-commit
        conn->setAutoCommit(false);

        // Generate a unique transaction ID
        std::string transactionId = generateUUID();

        // Create and store transaction context
        auto transContext = std::make_shared<TransactionContext>(conn, transactionId);

        // Lock and store in the active transactions map
        {
            std::lock_guard<std::mutex> lock(transactionMutex);
            activeTransactions[transactionId] = transContext;
        }

        return transactionId;
    }

    std::shared_ptr<Connection> TransactionManager::getTransactionConnection(const std::string &transactionId)
    {
        std::lock_guard<std::mutex> lock(transactionMutex);

        auto it = activeTransactions.find(transactionId);
        if (it == activeTransactions.end())
        {
            throw SQLException("Transaction not found: " + transactionId);
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
            std::lock_guard<std::mutex> lock(transactionMutex);

            auto it = activeTransactions.find(transactionId);
            if (it == activeTransactions.end())
            {
                throw SQLException("Transaction not found: " + transactionId);
            }

            transContext = it->second;
            activeTransactions.erase(it);
        }

        // Commit the transaction
        try
        {
            transContext->connection->commit();
            // Re-enable auto-commit before returning to pool
            transContext->connection->setAutoCommit(true);
            // Connection will be returned to pool when it goes out of scope
        }
        catch (const SQLException &e)
        {
            // Make sure to reset auto-commit even on error
            try
            {
                transContext->connection->setAutoCommit(true);
            }
            catch (...)
            {
                // Ignore errors during cleanup
            }
            throw; // Re-throw the original exception
        }
    }

    void TransactionManager::rollbackTransaction(const std::string &transactionId)
    {
        std::shared_ptr<TransactionContext> transContext;

        // Find and remove the transaction from the active map
        {
            std::lock_guard<std::mutex> lock(transactionMutex);

            auto it = activeTransactions.find(transactionId);
            if (it == activeTransactions.end())
            {
                throw SQLException("Transaction not found: " + transactionId);
            }

            transContext = it->second;
            activeTransactions.erase(it);
        }

        // Rollback the transaction
        try
        {
            transContext->connection->rollback();
            // Re-enable auto-commit before returning to pool
            transContext->connection->setAutoCommit(true);
            // Connection will be returned to pool when it goes out of scope
        }
        catch (const SQLException &e)
        {
            // Make sure to reset auto-commit even on error
            try
            {
                transContext->connection->setAutoCommit(true);
            }
            catch (...)
            {
                // Ignore errors during cleanup
            }
            throw; // Re-throw the original exception
        }
    }

    bool TransactionManager::isTransactionActive(const std::string &transactionId)
    {
        std::string expiredTransactionId;

        // First, check if the transaction exists and if it has timed out
        {
            std::lock_guard<std::mutex> lock(transactionMutex);

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
            catch (...)
            {
                // Ignore errors during cleanup
            }
            return false;
        }

        return true;
    }

    int TransactionManager::getActiveTransactionCount()
    {
        std::lock_guard<std::mutex> lock(transactionMutex);
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
            std::unique_lock<std::mutex> lock(cleanupMutex);
            cleanupCondition.wait_for(lock, std::chrono::milliseconds(cleanupIntervalMillis));

            if (!running)
            {
                break;
            }

            std::vector<std::string> expiredTransactions;

            // Find expired transactions
            {
                std::lock_guard<std::mutex> lock(transactionMutex);

                auto now = std::chrono::steady_clock::now();

                for (const auto &entry : activeTransactions)
                {
                    auto &transContext = entry.second;

                    auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - transContext->lastAccessTime)
                                        .count();

                    if (idleTime > transactionTimeoutMillis)
                    {
                        expiredTransactions.push_back(entry.first);
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
                catch (...)
                {
                    // Log or handle the error, but continue with other transactions
                }
            }
        }
    }

    std::string TransactionManager::generateUUID()
    {
        // Simple UUID generation using random numbers
        // In a production environment, consider using a proper UUID library

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);

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
        if (!running.exchange(false))
        {
            return; // Already closed
        }

        // Notify the cleanup thread to wake up and exit
        cleanupCondition.notify_all();

        // Join the cleanup thread
        if (cleanupThread.joinable())
        {
            cleanupThread.join();
        }

        // Rollback all active transactions
        std::vector<std::string> transactionIds;

        {
            std::lock_guard<std::mutex> lock(transactionMutex);
            for (const auto &entry : activeTransactions)
            {
                transactionIds.push_back(entry.first);
            }
        }

        for (const auto &transId : transactionIds)
        {
            try
            {
                rollbackTransaction(transId);
            }
            catch (...)
            {
                // Ignore errors during shutdown
            }
        }

        // Clear the transactions map
        std::lock_guard<std::mutex> lock(transactionMutex);
        activeTransactions.clear();
    }

} // namespace cpp_dbc
