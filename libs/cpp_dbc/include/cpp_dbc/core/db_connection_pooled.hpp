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
 * @file pooled_db_connection.hpp
 * @brief Abstract interface for pooled database connections
 */

#ifndef CPP_DBC_CORE_POOLED_DB_CONNECTION_HPP
#define CPP_DBC_CORE_POOLED_DB_CONNECTION_HPP

#include <memory>
#include <chrono>
#include "cpp_dbc/core/db_connection.hpp"

namespace cpp_dbc
{
    /**
     * @brief Abstract interface for pooled database connections
     *
     * Wraps a physical database connection with pool management metadata
     * (creation time, last used time, active state). Users typically interact
     * with pooled connections through the paradigm-specific interface
     * (e.g., RelationalDBConnection) and don't need to use this class directly.
     *
     * ```cpp
     * auto conn = pool->getDBConnection();
     * // conn is a DBConnectionPooled under the hood
     * std::cout << "Created: " << conn->getCreationTime().time_since_epoch().count() << std::endl;
     * std::cout << "Active: " << conn->isActive() << std::endl;
     * // Connection is returned automatically when it goes out of scope
     * ```
     *
     * @see DBConnectionPool, RelationalPooledDBConnection
     */
    class DBConnectionPooled : public DBConnection
    {
    protected:
        /**
         * @brief Check if the owning connection pool is still alive
         * @return true if the pool is still alive and valid
         */
        virtual bool isPoolValid() const = 0;

    public:
        ~DBConnectionPooled() override = default;

        /**
         * @brief Get the time when this pooled connection was created
         * @return The creation time point
         */
        virtual std::chrono::time_point<std::chrono::steady_clock> getCreationTime() const = 0;

        /**
         * @brief Get the last time this connection was borrowed from the pool
         * @return The last used time point
         */
        virtual std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime() const = 0;

        /**
         * @brief Set the active state of the connection
         * @param active Whether the connection is currently in use
         */
        virtual void setActive(bool active) = 0;

        /**
         * @brief Check if the connection is currently in use
         * @return true if the connection is active (borrowed)
         */
        virtual bool isActive() const = 0;

        /**
         * @brief Get the underlying physical database connection
         * @return A shared pointer to the unwrapped connection
         */
        virtual std::shared_ptr<DBConnection> getUnderlyingConnection() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_POOLED_DB_CONNECTION_HPP