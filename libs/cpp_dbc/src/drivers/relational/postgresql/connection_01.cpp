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
 @brief PostgreSQL database driver implementation - PostgreSQLDBConnection (constructor, destructor, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <charconv>

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // PostgreSQLDBConnection implementation

    // Private methods (in order of declaration in .hpp)

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
    void PostgreSQLDBConnection::registerStatement(std::weak_ptr<PostgreSQLDBPreparedStatement> stmt)
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
     * for API symmetry and potential future use.
     *
     * @param stmt Weak pointer to the statement to unregister
     */
    void PostgreSQLDBConnection::unregisterStatement(std::weak_ptr<PostgreSQLDBPreparedStatement> stmt)
    {
        std::scoped_lock lock(m_statementsMutex);
        // Remove expired weak_ptrs and the specified one
        auto stmtLocked = stmt.lock();
        std::erase_if(m_activeStatements, [&stmtLocked](const auto &w)
        {
            auto locked = w.lock();
            return !locked || (stmtLocked && locked.get() == stmtLocked.get());
        });
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
     * 3. The PreparedStatement destructor deallocates the statement on the server
     * 4. This uses the PGconn* connection that the new thread is also using
     * 5. Result: Race condition → protocol errors → potential corruption
     *
     * **The Solution:**
     *
     * By calling closeAllStatements() in returnToPool():
     * 1. All statements are closed BEFORE the connection becomes available to other threads
     * 2. Statement deallocation runs while we have exclusive access to the connection
     * 3. When PreparedStatement destructors run later, they find the statement already closed
     * 4. No server communication needed - safe!
     *
     * @note This method is called by returnToPool() and close()
     */
    void PostgreSQLDBConnection::closeAllStatements()
    {
        // CRITICAL: Must hold connection mutex to prevent other threads from using
        // the PGconn* connection while we close statements. Statement deallocation
        // (PQexec with DEALLOCATE) uses the connection internally, so concurrent
        // access causes protocol errors and potential corruption.
        // Note: m_statementsMutex is not needed here because registerStatement() is only
        // called from prepareStatement() which also holds m_connMutex, so we're protected.
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        for (auto &weak_stmt : m_activeStatements)
        {
            auto stmt = weak_stmt.lock();
            if (stmt)
            {
                // notifyConnClosing() calls close(std::nothrow) on the statement
                // This ensures statement deallocation while we have exclusive access
                stmt->notifyConnClosing();
            }
            // If weak_ptr is expired, statement was already destroyed - nothing to do
        }
        m_activeStatements.clear();
    }

    void PostgreSQLDBConnection::prepareForPoolReturn()
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

    // Constructor
    PostgreSQLDBConnection::PostgreSQLDBConnection(const std::string &host,
                                                   int port,
                                                   const std::string &database,
                                                   const std::string &user,
                                                   const std::string &password,
                                                   const std::map<std::string, std::string> &options)
        : m_closed(false)
    {
        // Build connection string
        std::stringstream conninfo;
        conninfo << "host=" << host << " ";
        conninfo << "port=" << port << " ";
        conninfo << "dbname=" << database << " ";
        conninfo << "user=" << user << " ";
        conninfo << "password=" << password << " ";

        for (const auto &[key, value] : options)
        {
            if (!key.starts_with("query__"))
            {
                conninfo << key << "=" << value << " ";
            }
        }

        if (!options.contains("gssencmode"))
        {
            conninfo << "gssencmode=disable";
        }

        // Connect to the database - create shared_ptr with custom deleter
        PGconn *rawConn = PQconnectdb(conninfo.str().c_str());
        if (PQstatus(rawConn) != CONNECTION_OK)
        {
            std::string error = PQerrorMessage(rawConn);
            PQfinish(rawConn);
            throw DBException("1Q2R3S4T5U6V", "Failed to connect to PostgreSQL: " + error, system_utils::captureCallStack());
        }

        // Wrap in shared_ptr with custom deleter
        m_conn = std::shared_ptr<PGconn>(rawConn, PGconnDeleter());

        // Set up a notice processor to suppress NOTICE messages
        PQsetNoticeProcessor(m_conn.get(), []([[maybe_unused]] void *arg, [[maybe_unused]] const char *message) // NOSONAR - void* signature required by PostgreSQL libpq API (PQnoticeProcessor typedef)
                             {
                                 // Do nothing with the notice message
                             },
                             nullptr);

        // Set auto-commit mode
        setAutoCommit(true);

        // Initialize URL string once
        std::stringstream urlBuilder;
        urlBuilder << "cpp_dbc:postgresql://" << host << ":" << port;
        if (!database.empty())
        {
            urlBuilder << "/" << database;
        }
        m_url = urlBuilder.str();
    }

    // Destructor
    PostgreSQLDBConnection::~PostgreSQLDBConnection()
    {
        PostgreSQLDBConnection::close();
    }

    // DBConnection interface
    void PostgreSQLDBConnection::close()
    {
        if (!m_closed && m_conn)
        {
            // Close all active statements before closing the connection
            // This ensures statement deallocation while we have exclusive access
            closeAllStatements();

            // Sleep for 25ms to avoid problems with concurrency
            std::this_thread::sleep_for(std::chrono::milliseconds(25));

            // shared_ptr will automatically call PQfinish via PGconnDeleter
            m_conn.reset();
            m_closed = true;
        }
    }

    bool PostgreSQLDBConnection::isClosed() const
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
     * 5. Destructor deallocates the prepared statement on the PostgreSQL server
     * 6. This uses the PGconn* connection that Thread B is also using
     * 7. RACE CONDITION: Two threads accessing same PGconn* simultaneously
     * 8. Result: Protocol errors, potential corruption, undefined behavior
     *
     * **With closeAllStatements():**
     * 1. Thread A creates connection, creates PreparedStatement, uses it
     * 2. Thread A calls conn->returnToPool()
     * 3. returnToPool() calls closeAllStatements() - ALL statements closed NOW
     * 4. Statement deallocation runs while Thread A still has exclusive access
     * 5. Connection goes back to pool (clean, no active statements)
     * 6. Thread B gets the connection - it's completely clean
     * 7. Thread A's PreparedStatement destructor finds statement already closed
     * 8. No server communication needed - safe!
     *
     * @note We don't set m_closed = true because the connection remains open
     * for reuse. Only the statements are closed.
     *
     * @see closeAllStatements() for the statement cleanup implementation
     * @see m_activeStatements for the design rationale of using weak_ptr
     */
    void PostgreSQLDBConnection::returnToPool()
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
            PG_DEBUG("PostgreSQLDBConnection::returnToPool - Exception during cleanup: " << ex.what());
        }
        catch (...) // NOSONAR - Intentional catch-all for non-std::exception types during cleanup
        {
            PG_DEBUG("PostgreSQLDBConnection::returnToPool - Unknown exception during cleanup");
        }
    }

    bool PostgreSQLDBConnection::isPooled()
    {
        return false;
    }

    std::string PostgreSQLDBConnection::getURL() const
    {
        return m_url;
    }

    // RelationalDBConnection interface
    std::shared_ptr<RelationalDBPreparedStatement> PostgreSQLDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBResultSet> PostgreSQLDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t PostgreSQLDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void PostgreSQLDBConnection::setAutoCommit(bool autoCommitFlag)
    {
        auto result = setAutoCommit(std::nothrow, autoCommitFlag);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool PostgreSQLDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBConnection::beginTransaction()
    {
        auto result = this->beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBConnection::transactionActive()
    {
        auto result = this->transactionActive(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void PostgreSQLDBConnection::commit()
    {
        auto result = this->commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBConnection::rollback()
    {
        auto result = this->rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = this->setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel PostgreSQLDBConnection::getTransactionIsolation()
    {
        auto result = this->getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBConnection::generateStatementName()
    {
        int counter = m_statementCounter;
        m_statementCounter += 1;
        std::stringstream ss;
        ss << "stmt_" << counter;
        return ss.str();
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
