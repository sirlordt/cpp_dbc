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
 * @file db_connection_pool.hpp
 * @brief Abstract interface for database connection pools
 */

#ifndef CPP_DBC_CORE_DB_CONNECTION_POOL_HPP
#define CPP_DBC_CORE_DB_CONNECTION_POOL_HPP

#include <memory>
#include <string>
#include <map>
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_types.hpp"

namespace cpp_dbc
{
    // Forward declarations
    class DBConnectionPooled;

    namespace config
    {
        class DBConnectionPoolConfig;
    }

    /**
     * @brief Abstract interface for database connection pools
     *
     * This class defines the common interface for all database connection pool implementations,
     * regardless of the specific database type (relational, document, etc.).
     *
     * DBConnectionPool is responsible for:
     * - Managing a pool of physical database connections
     * - Providing connections to clients upon request
     * - Handling connection lifecycle (creation, validation, recycling)
     * - Maintaining pool statistics
     */
    class DBConnectionPool
    {
    protected:
        /**
         * @brief PassKey idiom tag for enabling std::make_shared with protected constructors
         *
         * This empty struct serves as an access token that allows derived pool classes
         * to have "public" constructors that can only be called from within the class
         * hierarchy (since the tag is in the protected section). This enables the use of
         * std::make_shared while still enforcing the factory pattern.
         */
        struct ConstructorTag
        {
            ConstructorTag() = default;
        };

    protected:
        /**
         * @brief Set the transaction isolation level for all connections in the pool
         *
         * @param level The transaction isolation level to use
         */
        virtual void setPoolTransactionIsolation(TransactionIsolationLevel level) = 0;

    public:
        virtual ~DBConnectionPool() = default;

        /**
         * @brief Get a connection from the pool
         *
         * @return A shared pointer to a database connection
         */
        virtual std::shared_ptr<DBConnection> getDBConnection() = 0;

        /**
         * @brief Get the current number of active connections
         *
         * @return The number of connections currently in use
         */
        virtual size_t getActiveDBConnectionCount() const = 0;

        /**
         * @brief Get the current number of idle connections
         *
         * @return The number of connections currently idle in the pool
         */
        virtual size_t getIdleDBConnectionCount() const = 0;

        /**
         * @brief Get the total number of connections in the pool
         *
         * @return The total number of connections managed by this pool
         */
        virtual size_t getTotalDBConnectionCount() const = 0;

        /**
         * @brief Close the connection pool and all its connections
         *
         * After calling close(), the pool should not be used anymore.
         */
        virtual void close() = 0;

        /**
         * @brief Check if the connection pool is running
         *
         * @return true if the pool is running and can provide connections
         */
        virtual bool isRunning() const = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_CONNECTION_POOL_HPP