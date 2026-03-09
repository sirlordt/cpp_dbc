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
 @brief MySQL database driver implementation - MySQLDBConnection (private helpers, nothrow constructor, destructor, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include <array>
#include <charconv>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    // ============================================================================
    // MySQLDBConnection - Private Helper Methods
    // ============================================================================

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
    cpp_dbc::expected<void, DBException> MySQLDBConnection::registerStatement(std::nothrow_t, std::weak_ptr<MySQLDBPreparedStatement> stmt) noexcept
    {
        std::scoped_lock lock(m_statementsMutex);
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w)
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
     * for API symmetry and potential future use (e.g., if we want statements to
     * unregister themselves on close).
     *
     * @param stmt Weak pointer to the statement to unregister
     */
    cpp_dbc::expected<void, DBException> MySQLDBConnection::unregisterStatement(std::nothrow_t, std::weak_ptr<MySQLDBPreparedStatement> stmt) noexcept
    {
        std::scoped_lock lock(m_statementsMutex);
        // Remove expired weak_ptrs and the specified one
        auto stmtLocked = stmt.lock();
        std::erase_if(m_activeStatements, [&stmtLocked](const auto &w)
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
     * **Implementation Notes:**
     *
     * - Uses notifyConnClosing() which calls close(std::nothrow) on each statement
     * - Expired weak_ptrs (statement already destroyed) are simply skipped
     * - The registry is cleared after all statements are closed
     * - Connection mutex is held throughout to prevent concurrent modifications
     *
     * @note This method is called by returnToPool() and close()
     */
    cpp_dbc::expected<void, DBException> MySQLDBConnection::closeAllStatements(std::nothrow_t) noexcept
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
                // notifyConnClosing(std::nothrow) calls close(std::nothrow) on the statement
                // This ensures mysql_stmt_close() is called while we have exclusive access
                auto closeResult = stmt->notifyConnClosing(std::nothrow);
                if (!closeResult.has_value())
                {
                    MYSQL_DEBUG("MySQLDBConnection::closeAllStatements - notifyConnClosing failed: %s", closeResult.error().what_s().data());
                }
            }
            // If weak_ptr is expired, statement was already destroyed - nothing to do
        }
        m_activeStatements.clear();
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::registerResultSet(std::nothrow_t, std::weak_ptr<MySQLDBResultSet> rs) noexcept
    {
        std::scoped_lock lock(m_statementsMutex);
        if (m_activeResultSets.size() > 50)
        {
            std::erase_if(m_activeResultSets, [](const auto &w)
            {
                return w.expired();
            });
        }
        m_activeResultSets.insert(rs);
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::unregisterResultSet(std::nothrow_t, std::weak_ptr<MySQLDBResultSet> rs) noexcept
    {
        std::scoped_lock lock(m_statementsMutex);
        m_activeResultSets.erase(rs);
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::closeAllActiveResultSets(std::nothrow_t) noexcept
    {
        // Two-phase pattern: collect shared_ptrs under lock, then close outside
        // the lock to prevent iterator invalidation if close() triggers unregister
        std::vector<std::shared_ptr<MySQLDBResultSet>> resultSetsToClose;

        {
            std::scoped_lock lock(m_statementsMutex);
            for (auto &weakRs : m_activeResultSets)
            {
                if (auto rs = weakRs.lock())
                {
                    resultSetsToClose.push_back(rs);
                }
            }
            m_activeResultSets.clear();
        }

        for (const auto &rs : resultSetsToClose)
        {
            // notifyConnClosing(std::nothrow) marks the ResultSet as closed
            // This matches the pattern used by closeAllStatements() with PreparedStatements
            [[maybe_unused]] auto closeResult = rs->notifyConnClosing(std::nothrow);
            if (!closeResult.has_value())
            {
                MYSQL_DEBUG("MySQLDBConnection::closeAllActiveResultSets - notifyConnClosing failed: %s", closeResult.error().what_s().data());
            }
        }
        return {};
    }

    // ============================================================================
    // MySQLDBConnection - Nothrow Constructor + Destructor
    // ============================================================================

    MySQLDBConnection::MySQLDBConnection(PrivateCtorTag,
                                         std::nothrow_t,
                                         const std::string &host,
                                         int port,
                                         const std::string &database,
                                         const std::string &user,
                                         const std::string &password,
                                         const std::map<std::string, std::string> &options) noexcept
    {
        MYSQL_DEBUG("MySQLConnection::constructor(nothrow) - Creating connection to %s:%d/%s", host.c_str(), port, database.c_str());

        // Create shared_ptr with custom deleter for MYSQL*
        m_mysql = makeMySQLHandle(mysql_init(nullptr));
        if (!m_mysql)
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("FGGYHVGN7UGO", "Failed to initialize MySQL connection", system_utils::captureCallStack());
            return;
        }

        // Force TCP/IP connection
        unsigned int protocol = MYSQL_PROTOCOL_TCP;
        if (mysql_options(m_mysql.get(), MYSQL_OPT_PROTOCOL, &protocol) != 0)
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("VIH8G12GION4",
                "Failed to set MYSQL_OPT_PROTOCOL", system_utils::captureCallStack());
            return;
        }

        // Apply connection options from the map
        for (const auto &[key, value] : options)
        {
            if (key == "connect_timeout" || key == "read_timeout" || key == "write_timeout")
            {
                unsigned int timeout = 0;
                auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), timeout);
                if (ec != std::errc{} || ptr != value.data() + value.size())
                {
                    m_initFailed = true;
                    m_initError = std::make_unique<DBException>("0SYTWMSWWZZD",
                        "Invalid numeric value for option '" + key + "': " + value,
                        system_utils::captureCallStack());
                    return;
                }

                mysql_option opt = (key == "connect_timeout") ? MYSQL_OPT_CONNECT_TIMEOUT
                                 : (key == "read_timeout")    ? MYSQL_OPT_READ_TIMEOUT
                                                              : MYSQL_OPT_WRITE_TIMEOUT;
                if (mysql_options(m_mysql.get(), opt, &timeout) != 0)
                {
                    m_initFailed = true;
                    m_initError = std::make_unique<DBException>("6NO29VSVU60L",
                        "mysql_options failed for '" + key + "'",
                        system_utils::captureCallStack());
                    return;
                }
            }
            else if (key == "charset")
            {
                if (mysql_options(m_mysql.get(), MYSQL_SET_CHARSET_NAME, value.c_str()) != 0)
                {
                    m_initFailed = true;
                    m_initError = std::make_unique<DBException>("1PGUVACRCNF8",
                        "mysql_options failed for charset: " + value,
                        system_utils::captureCallStack());
                    return;
                }
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
            m_mysql.reset();
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("UEE2T50Q196S", "Failed to connect to MySQL: " + error, system_utils::captureCallStack());
            return;
        }

        // Select the database if provided
        if (!database.empty() && mysql_select_db(m_mysql.get(), database.c_str()) != 0)
        {
            std::string error = mysql_error(m_mysql.get());
            m_mysql.reset();
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("6I07T2DQL8XH", "Failed to select database: " + error, system_utils::captureCallStack());
            return;
        }

        // Enable auto-commit by default using the C API directly
        // (cannot call setAutoCommit() during construction — virtual dispatch not safe)
        if (mysql_autocommit(m_mysql.get(), true) != 0)
        {
            std::string error = mysql_error(m_mysql.get());
            m_mysql.reset();
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("WNZ2VGHWVLBA", "Failed to enable auto-commit: " + error, system_utils::captureCallStack());
            return;
        }
        m_autoCommit = true;

        // Initialize URL string once
        std::stringstream urlBuilder;
        urlBuilder << "cpp_dbc:mysql://" << host << ":" << port;
        if (!database.empty())
        {
            urlBuilder << "/" << database;
        }
        m_url = urlBuilder.str();

        // Mark connection as open
        m_closed.store(false, std::memory_order_release);

        MYSQL_DEBUG("MySQLConnection::constructor(nothrow) - Done, connected to %s", m_url.c_str());
    }

    MySQLDBConnection::~MySQLDBConnection()
    {
        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

    // ============================================================================
    // MySQLDBConnection - Throwing API Wrappers
    // ============================================================================

#ifdef __cpp_exceptions

    void MySQLDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MySQLDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void MySQLDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MySQLDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string MySQLDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
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

    bool MySQLDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBConnection::getServerVersion()
    {
        auto result = getServerVersion(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::map<std::string, std::string> MySQLDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
