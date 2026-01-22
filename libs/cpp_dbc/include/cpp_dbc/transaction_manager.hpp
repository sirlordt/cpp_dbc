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

    // Structure to hold transaction state
    struct TransactionContext
    {
        std::shared_ptr<RelationalDBConnection> connection;
        std::chrono::steady_clock::time_point creationTime;
        std::chrono::steady_clock::time_point lastAccessTime;
        std::string transactionId;
        bool active{false};

        TransactionContext(std::shared_ptr<RelationalDBConnection> conn, std::string id)
            : connection(conn),
              creationTime(std::chrono::steady_clock::now()),
              lastAccessTime(std::chrono::steady_clock::now()),
              transactionId(std::move(id)),
              active(true)
        {
        }
    };

    // Transaction manager class for relational databases
    class TransactionManager
    {
    private:
        RelationalDBConnectionPool &pool;
        std::map<std::string, std::shared_ptr<TransactionContext>> activeTransactions;
        std::mutex transactionMutex;
        std::jthread cleanupThread;
        std::atomic<bool> running{true}; // WARNING MUST BE TRUE. NOT Change to false
        std::condition_variable cleanupCondition;
        std::mutex cleanupMutex;

        // Configuration
        long transactionTimeoutMillis{300000}; // 5 minutes by default
        long cleanupIntervalMillis{60000};     // 1 minute by default

        // Helper methods
        void cleanupTask();
        std::string generateUUID() const;

    public:
        TransactionManager(RelationalDBConnectionPool &connectionPool);
        ~TransactionManager();

        // Start a new transaction and return its ID
        std::string beginTransaction();

        // Get a connection associated with a transaction
        std::shared_ptr<RelationalDBConnection> getTransactionDBConnection(const std::string &transactionId);

        // Commit a transaction by its ID
        void commitTransaction(const std::string &transactionId);

        // Rollback a transaction by its ID
        void rollbackTransaction(const std::string &transactionId);

        // Check if a transaction is active
        bool isTransactionActive(const std::string &transactionId);

        // Get the total number of active transactions
        size_t getActiveTransactionCount();

        // Set transaction timeout
        void setTransactionTimeout(long timeoutMillis);

        // Cleanup and shutdown
        void close();
    };

} // namespace cpp_dbc

#endif // CPP_DBC_TRANSACTION_MANAGER_HPP
