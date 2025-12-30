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
     * This class extends DBConnection with methods specific to pooled connections.
     * It wraps a physical database connection and adds pool management functionality.
     *
     * Implementations of this interface (e.g., RelationalPooledDBConnection, DocumentPooledDBConnection)
     * will provide the concrete behavior for connection pooling in their specific database paradigm.
     */
    class DBConnectionPooled : public DBConnection
    {
    protected:
        /**
         * @brief Check if the connection pool is still valid
         *
         * @return true if the pool is still alive and valid
         */
        virtual bool isPoolValid() const = 0;

    public:
        virtual ~DBConnectionPooled() = default;

        // The following methods are already defined in DBConnection and must be implemented
        // virtual void close() override = 0;
        // virtual bool isClosed() override = 0;
        // virtual void returnToPool() override = 0;
        // virtual bool isPooled() override = 0;
        // virtual std::string getURL() const override = 0;

        /**
         * @brief Get the creation time of this connection
         *
         * @return The time point when this connection was created
         */
        virtual std::chrono::time_point<std::chrono::steady_clock> getCreationTime() const = 0;

        /**
         * @brief Get the last time this connection was used
         *
         * @return The time point when this connection was last used
         */
        virtual std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime() const = 0;

        /**
         * @brief Set the active state of the connection
         *
         * @param active Whether the connection is currently active
         */
        virtual void setActive(bool active) = 0;

        /**
         * @brief Check if the connection is active
         *
         * @return true if the connection is currently active
         */
        virtual bool isActive() const = 0;

        /**
         * @brief Get the underlying physical connection
         *
         * @return A shared pointer to the underlying database connection
         */
        virtual std::shared_ptr<DBConnection> getUnderlyingConnection() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_POOLED_DB_CONNECTION_HPP