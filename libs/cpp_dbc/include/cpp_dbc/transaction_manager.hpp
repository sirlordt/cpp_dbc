// CPPDBC_TransactionManager.hpp
// Transaction manager for cross-thread transaction management

#ifndef CPP_DBC_TRANSACTION_MANAGER_HPP
#define CPP_DBC_TRANSACTION_MANAGER_HPP

#include "cpp_dbc.hpp"
#include "connection_pool.hpp"
#include <map>
#include <mutex>
#include <string>
#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include <functional>

namespace cpp_dbc
{

    // Structure to hold transaction state
    struct TransactionContext
    {
        std::shared_ptr<Connection> connection;
        std::chrono::steady_clock::time_point creationTime;
        std::chrono::steady_clock::time_point lastAccessTime;
        std::string transactionId;
        bool active;

        TransactionContext(std::shared_ptr<Connection> conn, std::string id)
            : connection(conn),
              creationTime(std::chrono::steady_clock::now()),
              lastAccessTime(std::chrono::steady_clock::now()),
              transactionId(std::move(id)),
              active(true)
        {
        }
    };

    // Transaction manager class
    class TransactionManager
    {
    private:
        ConnectionPool &pool;
        std::map<std::string, std::shared_ptr<TransactionContext>> activeTransactions;
        std::mutex transactionMutex;
        std::thread cleanupThread;
        std::atomic<bool> running;

        // Configuration
        long transactionTimeoutMillis = 300000; // 5 minutes by default
        long cleanupIntervalMillis = 60000;     // 1 minute by default

        // Helper methods
        void cleanupTask();
        std::string generateUUID();

    public:
        TransactionManager(ConnectionPool &connectionPool);
        ~TransactionManager();

        // Start a new transaction and return its ID
        std::string beginTransaction();

        // Get a connection associated with a transaction
        std::shared_ptr<Connection> getTransactionConnection(const std::string &transactionId);

        // Commit a transaction by its ID
        void commitTransaction(const std::string &transactionId);

        // Rollback a transaction by its ID
        void rollbackTransaction(const std::string &transactionId);

        // Check if a transaction is active
        bool isTransactionActive(const std::string &transactionId);

        // Get the total number of active transactions
        int getActiveTransactionCount();

        // Set transaction timeout
        void setTransactionTimeout(long timeoutMillis);

        // Cleanup and shutdown
        void close();
    };

} // namespace cpp_dbc

#endif // CPP_DBC_TRANSACTION_MANAGER_HPP
