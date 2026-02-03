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

 @file connection_01.cpp
 @brief MySQL database driver implementation - MySQLDBConnection (constructor, destructor, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include "cpp_dbc/drivers/relational/mysql_blob.hpp"
#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    // MySQLDBConnection implementation

    /**
     * @brief Register a prepared statement in the active statements registry
     *
     * @details
     * This method is called automatically when a new PreparedStatement is created
     * via prepareStatement(). The statement is stored as a weak_ptr to allow
     * natural destruction when the user releases their reference.
     *
     * @param stmt Weak pointer to the statement to register
     *
     * @see closeAllStatements() for cleanup logic
     * @see m_activeStatements for design rationale
     */
    void MySQLDBConnection::registerStatement(std::weak_ptr<MySQLDBPreparedStatement> stmt)
    {
        std::scoped_lock lock(m_statementsMutex);
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w) { return w.expired(); });
        }
        m_activeStatements.insert(stmt);
    }

    /**
     * @brief Unregister a prepared statement from the active statements registry
     *
     * @details
     * This method removes a specific statement from the registry. It also cleans up
     * any expired weak_ptrs encountered during iteration.
     *
     * @note Currently unused - statements are cleaned up via closeAllStatements()
     * in returnToPool() and close(), or they expire naturally. This method is kept
     * for API symmetry and potential future use (e.g., if we want statements to
     * unregister themselves on close).
     *
     * @param stmt Weak pointer to the statement to unregister
     */
    void MySQLDBConnection::unregisterStatement(std::weak_ptr<MySQLDBPreparedStatement> stmt)
    {
        std::scoped_lock lock(m_statementsMutex);
        // Remove expired weak_ptrs and the specified one
        for (auto it = m_activeStatements.begin(); it != m_activeStatements.end();)
        {
            auto locked = it->lock();
            auto stmtLocked = stmt.lock();
            if (!locked || (stmtLocked && locked.get() == stmtLocked.get()))
            {
                it = m_activeStatements.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    /**
     * @brief Close all active prepared statements
     *
     * @details
     * **CRITICAL FOR THREAD SAFETY IN CONNECTION POOLING**
     *
     * This method iterates through all registered statements and explicitly closes them.
     * This is essential to prevent race conditions when using connection pools.
     *
     * **The Problem It Solves:**
     *
     * Without this method, when a connection is returned to the pool:
     * 1. Another thread may obtain the connection and start using it
     * 2. Meanwhile, the original user's PreparedStatement references may be destroyed
     * 3. The PreparedStatement destructor calls mysql_stmt_close()
     * 4. mysql_stmt_close() communicates with the MySQL server using the MYSQL* connection
     * 5. But the new thread is also using that same MYSQL* connection
     * 6. Result: Race condition → use-after-free → memory corruption
     *
     * **The Solution:**
     *
     * By calling closeAllStatements() in returnToPool():
     * 1. All statements are closed BEFORE the connection becomes available to other threads
     * 2. mysql_stmt_close() runs while we have exclusive access to the connection
     * 3. When PreparedStatement destructors run later, they find the statement already closed
     * 4. No mysql_stmt_close() call is made (it's a no-op on already-closed statements)
     * 5. No race condition possible
     *
     * **Implementation Notes:**
     *
     * - Uses notifyConnClosing() which calls close(std::nothrow) on each statement
     * - Expired weak_ptrs (statement already destroyed) are simply skipped
     * - The registry is cleared after all statements are closed
     * - Lock is held throughout to prevent concurrent modifications
     *
     * @note This method is called by returnToPool() and close()
     *
     * **IMPORTANT THREADING NOTE:**
     *
     * This method acquires BOTH m_connMutex (connection mutex) AND m_statementsMutex.
     * The connection mutex ensures no other thread is using the MYSQL* connection
     * while we call mysql_stmt_close() on each statement. Without this, another thread
     * could be executing a query on the connection while we're closing statements,
     * leading to use-after-free errors because mysql_stmt_close() and mysql_query()
     * both access shared internal structures in libmysqlclient.
     */
    void MySQLDBConnection::closeAllStatements()
    {
        // CRITICAL: Must hold connection mutex to prevent other threads from using
        // the MYSQL* connection while we close statements. mysql_stmt_close() uses
        // the connection internally, so concurrent access causes corruption.
        // Note: m_statementsMutex is not needed here because registerStatement() is only
        // called from prepareStatement() which also holds m_connMutex, so we're protected.
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        for (auto &weak_stmt : m_activeStatements)
        {
            auto stmt = weak_stmt.lock();
            if (stmt)
            {
                // notifyConnClosing() calls close(std::nothrow) on the statement
                // This ensures mysql_stmt_close() is called while we have exclusive access
                stmt->notifyConnClosing();
            }
            // If weak_ptr is expired, statement was already destroyed - nothing to do
        }
        m_activeStatements.clear();
    }

    void MySQLDBConnection::prepareForPoolReturn()
    {
        // Close all active statements first
        closeAllStatements();

        // Rollback any active transaction
        auto txActive = transactionActive(std::nothrow);
        if (txActive.has_value() && txActive.value())
        {
            rollback(std::nothrow);
        }

        // Reset auto-commit to true
        setAutoCommit(std::nothrow, true);
    }

    MySQLDBConnection::MySQLDBConnection(const std::string &host,
                                         int port,
                                         const std::string &database,
                                         const std::string &user,
                                         const std::string &password,
                                         const std::map<std::string, std::string> &options)
        : m_closed(false)
#if DB_DRIVER_THREAD_SAFE
        , m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
    {
        // Create shared_ptr with custom deleter for MYSQL*
        m_mysql = std::shared_ptr<MYSQL>(mysql_init(nullptr), MySQLDeleter());
        if (!m_mysql)
        {
            throw DBException("N3Z4A5B6C7D8", "Failed to initialize MySQL connection", system_utils::captureCallStack());
        }

        // Force TCP/IP connection
        unsigned int protocol = MYSQL_PROTOCOL_TCP;
        mysql_options(m_mysql.get(), MYSQL_OPT_PROTOCOL, &protocol);

        // Aplicar opciones de conexión desde el mapa
        for (const auto &[key, value] : options)
        {
            if (key == "connect_timeout")
            {
                unsigned int timeout = std::stoi(value);
                mysql_options(m_mysql.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
            }
            else if (key == "read_timeout")
            {
                unsigned int timeout = std::stoi(value);
                mysql_options(m_mysql.get(), MYSQL_OPT_READ_TIMEOUT, &timeout);
            }
            else if (key == "write_timeout")
            {
                unsigned int timeout = std::stoi(value);
                mysql_options(m_mysql.get(), MYSQL_OPT_WRITE_TIMEOUT, &timeout);
            }
            else if (key == "charset")
            {
                mysql_options(m_mysql.get(), MYSQL_SET_CHARSET_NAME, value.c_str());
            }
            else if (key == "auto_reconnect" && value == "true")
            {
                bool reconnect = true;
                mysql_options(m_mysql.get(), MYSQL_OPT_RECONNECT, &reconnect);
            }
        }

        // Connect to the database
        if (!mysql_real_connect(m_mysql.get(), host.c_str(), user.c_str(), password.c_str(),
                                nullptr, port, nullptr, 0))
        {
            std::string error = mysql_error(m_mysql.get());
            // unique_ptr will automatically call mysql_close via the deleter
            m_mysql.reset();
            throw DBException("N4Z5A6B7C8D9", "Failed to connect to MySQL: " + error, system_utils::captureCallStack());
        }

        // Select the database if provided
        if (!database.empty() && mysql_select_db(m_mysql.get(), database.c_str()) != 0)
        {
            std::string error = mysql_error(m_mysql.get());
            // unique_ptr will automatically call mysql_close via the deleter
            m_mysql.reset();
            throw DBException("N5Z6A7B8C9D0", "Failed to select database: " + error, system_utils::captureCallStack());
        }

        // Enable auto-commit by default
        setAutoCommit(true); // NOSONAR - safe: during construction, dynamic type is exactly MySQLDBConnection

        // Initialize URL string once
        std::stringstream urlBuilder;
        urlBuilder << "cpp_dbc:mysql://" << host << ":" << port;
        if (!database.empty())
        {
            urlBuilder << "/" << database;
        }
        m_url = urlBuilder.str();
    }

    MySQLDBConnection::~MySQLDBConnection()
    {
        MySQLDBConnection::close();
    }

    void MySQLDBConnection::close()
    {
        if (!m_closed && m_mysql)
        {
            // Close all active statements before closing the connection
            // This ensures mysql_stmt_close() is called while we have exclusive access
            closeAllStatements();

            // Sleep for 25ms to avoid problems with concurrency
            std::this_thread::sleep_for(std::chrono::milliseconds(25));

            m_mysql.reset();
            m_closed = true;
        }
    }

    bool MySQLDBConnection::isClosed() const
    {
        return m_closed;
    }

    /**
     * @brief Return this connection to the connection pool for reuse
     *
     * @details
     * **CRITICAL: Statement Cleanup Before Pool Return**
     *
     * This method prepares the connection for reuse by another thread. The most
     * important step is closing all active prepared statements BEFORE the connection
     * becomes available to other threads.
     *
     * **Why closeAllStatements() is Essential Here:**
     *
     * Consider this scenario WITHOUT closeAllStatements():
     * 1. Thread A creates connection, creates PreparedStatement, uses it
     * 2. Thread A calls conn->returnToPool() - connection goes back to pool
     * 3. Thread B gets the same connection from pool, starts using it
     * 4. Thread A's PreparedStatement goes out of scope, destructor runs
     * 5. Destructor calls mysql_stmt_close() on Thread A's statement
     * 6. mysql_stmt_close() uses the MYSQL* connection that Thread B is using
     * 7. RACE CONDITION: Two threads accessing same MYSQL* simultaneously
     * 8. Result: Memory corruption, crashes, undefined behavior
     *
     * **With closeAllStatements():**
     * 1. Thread A creates connection, creates PreparedStatement, uses it
     * 2. Thread A calls conn->returnToPool()
     * 3. returnToPool() calls closeAllStatements() - ALL statements closed NOW
     * 4. mysql_stmt_close() runs while Thread A still has exclusive access
     * 5. Connection goes back to pool (clean, no active statements)
     * 6. Thread B gets the connection - it's completely clean
     * 7. Thread A's PreparedStatement destructor finds statement already closed
     * 8. No mysql_stmt_close() call needed - safe!
     *
     * @note We don't set m_closed = true because the connection remains open
     * for reuse. Only the statements are closed.
     *
     * @see closeAllStatements() for the statement cleanup implementation
     * @see m_activeStatements for the design rationale of using weak_ptr
     */
    void MySQLDBConnection::returnToPool()
    {
        try
        {
            // CRITICAL: Close all active statements BEFORE making connection available
            // This prevents race conditions when another thread gets this connection
            closeAllStatements();

            // Restore autocommit for the next user of this connection
            if (!m_autoCommit)
            {
                setAutoCommit(true);
            }

            // Note: We don't set m_closed = true because the connection remains open
            // for reuse by the pool. Only the statements are closed.
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            // Ignore errors during cleanup - connection may still be usable
            MYSQL_DEBUG("MySQLDBConnection::returnToPool - Exception ignored during cleanup: " << ex.what());
        }
    }

    bool MySQLDBConnection::isPooled()
    {
        return false;
    }

    std::string MySQLDBConnection::getURL() const
    {
        return m_url;
    }

    std::shared_ptr<RelationalDBPreparedStatement> MySQLDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<RelationalDBResultSet> MySQLDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t MySQLDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MySQLDBConnection::setAutoCommit(bool autoCommitFlag)
    {
        auto result = setAutoCommit(std::nothrow, autoCommitFlag);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MySQLDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBConnection::transactionActive()
    {
        auto result = transactionActive(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MySQLDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel MySQLDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
