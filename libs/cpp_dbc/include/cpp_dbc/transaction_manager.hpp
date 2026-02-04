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

 @file transaction_manager.hpp
 @brief Tests for transaction management

*/

#ifndef CPP_DBC_TRANSACTION_MANAGER_HPP
#define CPP_DBC_TRANSACTION_MANAGER_HPP

#include "cpp_dbc.hpp"
#include "core/relational/relational_db_connection_pool.hpp"
#include <map>
#include <mutex>
#include <string>
#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include <functional>
#include <condition_variable>

namespace cpp_dbc
{

    /**
     * @brief Holds the state of an active transaction
     *
     * Tracks the connection, creation time, last access time, and active status
     * for a managed transaction.
     */
    struct TransactionContext
    {
        std::shared_ptr<RelationalDBConnection> connection;
        std::chrono::steady_clock::time_point creationTime{std::chrono::steady_clock::now()};
        std::chrono::steady_clock::time_point lastAccessTime{std::chrono::steady_clock::now()};
        std::string transactionId;
        bool active{false};

        TransactionContext(std::shared_ptr<RelationalDBConnection> conn, std::string id)
            : connection(conn),
              transactionId(std::move(id)),
              active(true)
        {
        }
    };

    /**
     * @brief Manages database transactions with automatic cleanup and timeout
     *
     * Provides named transaction management on top of a RelationalDBConnectionPool.
     * Transactions are identified by UUID and automatically cleaned up when they
     * exceed the configured timeout.
     *
     * ```cpp
     * cpp_dbc::TransactionManager txMgr(pool);
     * txMgr.setTransactionTimeout(60000); // 60 seconds
     * std::string txId = txMgr.beginTransaction();
     * auto conn = txMgr.getTransactionDBConnection(txId);
     * conn->executeUpdate("INSERT INTO users (name) VALUES ('Alice')");
     * txMgr.commitTransaction(txId);
     * txMgr.close();
     * ```
     *
     * @see RelationalDBConnectionPool
     */
    class TransactionManager
    {
    private:
        RelationalDBConnectionPool &pool;
        std::map<std::string, std::shared_ptr<TransactionContext>> activeTransactions;
        std::mutex transactionMutex;
        // Note: Declaration order matters for initialization - these must come before cleanupThread
        std::atomic<bool> running{true}; // WARNING MUST BE TRUE. NOT Change to false
        std::condition_variable cleanupCondition;
        std::mutex cleanupMutex;

        // Configuration - must be declared before cleanupThread since it uses cleanupIntervalMillis
        long transactionTimeoutMillis{300000}; // 5 minutes by default
        long cleanupIntervalMillis{60000};     // 1 minute by default

        std::jthread cleanupThread;  // Must be declared after running/cleanupCondition/cleanupMutex/cleanupIntervalMillis

        // Helper methods
        void cleanupTask();
        std::string generateUUID() const;

    public:
        explicit TransactionManager(RelationalDBConnectionPool &connectionPool);
        ~TransactionManager();

        /** @brief Start a new transaction and return its UUID identifier */
        std::string beginTransaction();

        /** @brief Get the connection associated with a transaction by its ID */
        std::shared_ptr<RelationalDBConnection> getTransactionDBConnection(const std::string &transactionId);

        /** @brief Commit a transaction by its UUID identifier */
        void commitTransaction(const std::string &transactionId);

        /** @brief Rollback a transaction by its UUID identifier */
        void rollbackTransaction(const std::string &transactionId);

        /** @brief Check if a transaction is active by its UUID identifier */
        bool isTransactionActive(const std::string &transactionId);

        /** @brief Get the total number of active transactions */
        size_t getActiveTransactionCount();

        /** @brief Set the transaction timeout in milliseconds */
        void setTransactionTimeout(long timeoutMillis);

        /** @brief Close the transaction manager and rollback all active transactions */
        void close();
    };

} // namespace cpp_dbc

#endif // CPP_DBC_TRANSACTION_MANAGER_HPP
