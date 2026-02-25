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
#include <new> // For std::nothrow_t
#include "db_expected.hpp"
#include "db_exception.hpp"

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
     *     "cpp_dbc:mysql://localhost:3306/mydb", "user", "pass");
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
        virtual bool isPooled() const = 0;

        /**
         * @brief Get the connection URL
         *
         * Returns the URL used to establish this connection, including
         * the connection type and parameters.
         *
         * @return std::string The connection URL (e.g., "jdbc:mysql://localhost:3306/mydb")
         */
        virtual std::string getURL() const = 0;

        /**
         * @brief Reset the connection state
         *
         * Closes all active prepared statements and result sets, rolls back
         * any active transaction, and resets auto-commit to true. Called
         * internally by close() and prepareForPoolReturn().
         *
         * @throws DBException if reset fails
         */
        virtual void reset() = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Close the database connection (nothrow version)
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept = 0;

        /**
         * @brief Reset the connection state (nothrow version)
         *
         * Called by close() and prepareForPoolReturn() to ensure clean state.
         *
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException> reset(std::nothrow_t) noexcept = 0;

        /**
         * @brief Check if the connection is closed (nothrow version)
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing true if closed, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept = 0;

        /**
         * @brief Return the connection to its connection pool (nothrow version)
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept = 0;

        /**
         * @brief Check if the connection is managed by a pool (nothrow version)
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing true if pooled, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept = 0;

        /**
         * @brief Get the connection URL (nothrow version)
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing the connection URL string, or DBException on failure
         */
        virtual cpp_dbc::expected<std::string, DBException> getURL(std::nothrow_t) const noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_CONNECTION_HPP