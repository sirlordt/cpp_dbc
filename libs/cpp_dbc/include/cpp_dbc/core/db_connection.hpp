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
 * @file db_connection.hpp
 * @brief Abstract base class for all database connections
 */

#ifndef CPP_DBC_CORE_DB_CONNECTION_HPP
#define CPP_DBC_CORE_DB_CONNECTION_HPP

#include <string>

namespace cpp_dbc
{

    /**
     * @brief Abstract base class for all database connections
     *
     * This is the root of the connection hierarchy. All database paradigms
     * (relational, document, key-value, columnar) derive their connection
     * classes from this base class.
     *
     * Connections are obtained via DriverManager::getDBConnection() and must
     * be cast to the appropriate paradigm-specific subclass for database operations.
     *
     * ```cpp
     * // Obtain a connection and use it
     * auto conn = cpp_dbc::DriverManager::getDBConnection(
     *     "jdbc:mysql://localhost:3306/mydb", "user", "pass");
     * std::cout << "Connected to: " << conn->getURL() << std::endl;
     * // ... use paradigm-specific subclass methods ...
     * conn->close();
     * ```
     *
     * @see RelationalDBConnection, DocumentDBConnection, KVDBConnection, ColumnarDBConnection
     */
    class DBConnection
    {
    public:
        virtual ~DBConnection() = default;

        /**
         * @brief Close the database connection and release resources
         *
         * After calling close(), the connection should not be used.
         * Implementations handle multiple calls to close() gracefully.
         *
         * ```cpp
         * auto conn = cpp_dbc::DriverManager::getDBConnection(url, user, pass);
         * // ... work with connection ...
         * conn->close();  // release resources
         * ```
         */
        virtual void close() = 0;

        /**
         * @brief Check if the connection is closed
         *
         * @return true if the connection is closed
         * @return false if the connection is still open
         *
         * ```cpp
         * if (!conn->isClosed()) {
         *     conn->close();
         * }
         * ```
         */
        virtual bool isClosed() const = 0;

        /**
         * @brief Return the connection to its connection pool
         *
         * If the connection is managed by a pool, this method returns it
         * to the pool for reuse. If not pooled, this may close the connection.
         * Prefer this over close() for pooled connections.
         *
         * ```cpp
         * auto conn = pool->getDBConnection();
         * // ... use connection ...
         * conn->returnToPool();  // return to pool instead of closing
         * ```
         */
        virtual void returnToPool() = 0;

        /**
         * @brief Check if the connection is managed by a connection pool
         *
         * @return true if the connection is pooled
         * @return false if the connection is standalone
         */
        virtual bool isPooled() = 0;

        /**
         * @brief Get the connection URL
         *
         * Returns the URL used to establish this connection, including
         * the connection type and parameters.
         *
         * @return std::string The connection URL (e.g., "jdbc:mysql://localhost:3306/mydb")
         */
        virtual std::string getURL() const = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_CONNECTION_HPP