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
    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::registerStatement(std::nothrow_t, std::weak_ptr<PostgreSQLDBPreparedStatement> stmt) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("OC3B7VMPTYQ0", "Cannot register statement");
        std::scoped_lock stmtLock(m_statementsMutex);
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements,
                          [](const auto &w)
                          {
                              return w.expired();
                          });
        }
        m_activeStatements.insert(stmt);
        return {};
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
    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::unregisterStatement(std::nothrow_t, std::weak_ptr<PostgreSQLDBPreparedStatement> stmt) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("8HARHU7XSIGK", "Cannot unregister statement");
        std::scoped_lock stmtLock(m_statementsMutex);
        // Remove expired weak_ptrs and the specified one
        auto stmtLocked = stmt.lock();
        std::erase_if(m_activeStatements,
                      [&stmtLocked](const auto &w)
                      {
                          auto locked = w.lock();
                          return !locked || (stmtLocked && locked.get() == stmtLocked.get());
                      });
        return {};
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
    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::closeAllStatements(std::nothrow_t) noexcept
    {
        // CRITICAL: Must hold connection mutex to prevent other threads from using
        // the PGconn* connection while we close statements. Statement deallocation
        // (PQexec with DEALLOCATE) uses the connection internally, so concurrent
        // access causes protocol errors and potential corruption.
        // Note: m_statementsMutex is not needed here because registerStatement() is only
        // called from prepareStatement() which also holds m_connMutex, so we're protected.
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("8ZCFAYQKKLXU", "Cannot close statements");

        for (auto &weak_stmt : m_activeStatements)
        {
            auto stmt = weak_stmt.lock();
            if (stmt)
            {
                // notifyConnClosing() calls close(std::nothrow) on the statement
                // This ensures statement deallocation while we have exclusive access
                stmt->notifyConnClosing(std::nothrow);
            }
            // If weak_ptr is expired, statement was already destroyed - nothing to do
        }
        m_activeStatements.clear();
        return {};
    }

    /**
     * @brief Register a result set in the active result sets registry
     *
     * @details
     * This method is called automatically when a new ResultSet is created
     * via executeQuery(). The result set is stored as a weak_ptr to allow
     * natural destruction when the user releases their reference.
     *
     * @param rs Weak pointer to the result set to register
     *
     * @see closeAllResultSets() for cleanup logic
     * @see m_activeResultSets for design rationale
     */
    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::registerResultSet(std::nothrow_t, std::weak_ptr<PostgreSQLDBResultSet> rs) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("21WYRCIB636U", "Cannot register result set");
        std::scoped_lock stmtLock(m_statementsMutex);
        if (m_activeResultSets.size() > 50)
        {
            std::erase_if(m_activeResultSets,
                          [](const auto &w)
                          {
                              return w.expired();
                          });
        }
        m_activeResultSets.insert(rs);
        return {};
    }

    /**
     * @brief Unregister a result set from the active result sets registry
     *
     * @param rs Weak pointer to the result set to unregister
     */
    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::unregisterResultSet(std::nothrow_t, std::weak_ptr<PostgreSQLDBResultSet> rs) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("0VSIXJXK2CSE", "Cannot unregister result set");
        std::scoped_lock stmtLock(m_statementsMutex);
        auto rsLocked = rs.lock();
        std::erase_if(m_activeResultSets,
                      [&rsLocked](const auto &w)
                      {
                          auto locked = w.lock();
                          return !locked || (rsLocked && locked.get() == rsLocked.get());
                      });
        return {};
    }

    /**
     * @brief Close all active result sets
     *
     * @details
     * Mirrors closeAllStatements(). When the connection closes or is returned
     * to the pool, all active result sets are notified and closed. This ensures
     * consistent lifecycle management across all drivers.
     *
     * @note This method is called by returnToPool(), reset(), and close()
     */
    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::closeAllResultSets(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("NTKCA6GKJCAE", "Cannot close result sets");

        for (auto &weak_rs : m_activeResultSets)
        {
            auto rs = weak_rs.lock();
            if (rs)
            {
                rs->notifyConnClosing(std::nothrow);
            }
        }
        m_activeResultSets.clear();
        return {};
    }

    std::string PostgreSQLDBConnection::formatServerVersion(std::nothrow_t, int version) const noexcept
    {
        if (version >= 100000)
        {
            int major = version / 10000;
            int minor = version % 10000;
            return std::to_string(major) + "." + std::to_string(minor);
        }
        // Pre-10: major*10000 + minor*100 + patch
        int major = version / 10000;
        int minor = (version / 100) % 100;
        int patch = version % 100;
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    std::string PostgreSQLDBConnection::generateStatementName(std::nothrow_t) noexcept
    {
        int counter = m_statementCounter;
        m_statementCounter += 1;
        return "stmt_" + std::to_string(counter);
    }

    // Constructor — nothrow, captures errors in m_initFailed / m_initError instead of throwing.
    // PQconnectdb() is a C API that does not throw, but setAutoCommit() calls PQexec() which
    // is also a C API. No recoverable C++ exceptions are possible here, so no try/catch needed.
    PostgreSQLDBConnection::PostgreSQLDBConnection(PostgreSQLDBConnection::PrivateCtorTag,
                                                   std::nothrow_t,
                                                   const std::string &host,
                                                   int port,
                                                   const std::string &database,
                                                   const std::string &user,
                                                   const std::string &password,
                                                   const std::map<std::string, std::string> &options) noexcept
    {
        // Validate database name — only alphanumeric and underscores allowed
        if (!database.empty() && !system_utils::isValidDatabaseIdentifier(database))
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("UB41BT6RLFVM",
                                                        "Invalid PostgreSQL database name: only alphanumeric and underscore allowed",
                                                        system_utils::captureCallStack());
            return;
        }

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
        if (!rawConn)
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("GLN3SP411SYW",
                                                        "Failed to allocate PGconn (out of memory)",
                                                        system_utils::captureCallStack());
            return;
        }
        if (PQstatus(rawConn) != CONNECTION_OK)
        {
            std::string error = PQerrorMessage(rawConn);
            PQfinish(rawConn);
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("GLN3SP411SYV",
                                                        "Failed to connect to PostgreSQL: " + error,
                                                        system_utils::captureCallStack());
            return;
        }

        // Wrap in shared_ptr with custom deleter
        m_conn = std::shared_ptr<PGconn>(rawConn, PGconnDeleter());

        // Set up a notice processor to suppress NOTICE messages
        PQsetNoticeProcessor(m_conn.get(), []([[maybe_unused]] void *arg, [[maybe_unused]] const char *message) // NOSONAR(cpp:S5008) — void* signature required by PostgreSQL libpq API (PQnoticeProcessor typedef)
                             {
                                 // Do nothing with the notice message
                             },
                             nullptr);

        // Set auto-commit mode (nothrow — errors silently ignored in constructor;
        // the connection is still valid if autocommit setup fails)
        [[maybe_unused]] auto acResult = setAutoCommit(std::nothrow, true);

        // Initialize URI string once (reuse centralized builder for IPv6 bracket handling)
        m_uri = system_utils::buildDBURI("cpp_dbc:postgresql://", host, port, database);

        m_closed.store(false, std::memory_order_seq_cst);
    }

    // Destructor
    PostgreSQLDBConnection::~PostgreSQLDBConnection()
    {
        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

#ifdef __cpp_exceptions

    // DBConnection interface
    void PostgreSQLDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool PostgreSQLDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void PostgreSQLDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool PostgreSQLDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBConnection::getURI() const
    {
        auto result = getURI(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
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

    std::string PostgreSQLDBConnection::getServerVersion()
    {
        auto result = getServerVersion(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::map<std::string, std::string> PostgreSQLDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
