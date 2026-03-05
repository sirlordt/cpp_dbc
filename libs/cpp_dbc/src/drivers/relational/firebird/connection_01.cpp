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

 @file connection_01.cpp
 @brief Firebird database driver implementation - FirebirdDBConnection constructor, private nothrow helpers, destructor, and throwing API

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    // ============================================================================
    // FirebirdDBConnection Implementation
    // ============================================================================

    // Nothrow constructor — all connection logic lives here.
    // On failure, sets m_initFailed/m_initError instead of throwing.
    // Public for std::make_shared access, but effectively private via PrivateCtorTag.
    FirebirdDBConnection::FirebirdDBConnection(FirebirdDBConnection::PrivateCtorTag,
                                               std::nothrow_t,
                                               const std::string &host,
                                               int port,
                                               const std::string &database,
                                               const std::string &user,
                                               const std::string &password,
                                               const std::map<std::string, std::string> &options) noexcept
        : m_tr(0), m_isolationLevel(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED)
#if DB_DRIVER_THREAD_SAFE
          ,
          m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
    {
        try
        {
            FIREBIRD_DEBUG("FirebirdConnection::constructor - Starting");
            FIREBIRD_DEBUG("  host: %s", host.c_str());
            FIREBIRD_DEBUG("  port: %d", port);
            FIREBIRD_DEBUG("  database: %s", database.c_str());
            FIREBIRD_DEBUG("  user: %s", user.c_str());

            ISC_STATUS_ARRAY status;

            // Build connection string
            std::string connStr;
            if (!host.empty() && host != "localhost" && host != "127.0.0.1")
            {
                connStr = host;
                if (port != 3050 && port != 0)
                {
                    connStr += "/" + std::to_string(port);
                }
                connStr += ":";
            }
            connStr += database;
            FIREBIRD_DEBUG("  Connection string: %s", connStr.c_str());

            // Build DPB (Database Parameter Block)
            std::vector<char> dpb;
            dpb.push_back(isc_dpb_version1);

            // Add user name
            dpb.push_back(isc_dpb_user_name);
            dpb.push_back(static_cast<char>(user.length()));
            dpb.insert(dpb.end(), user.begin(), user.end());

            // Add password
            dpb.push_back(isc_dpb_password);
            dpb.push_back(static_cast<char>(password.length()));
            dpb.insert(dpb.end(), password.begin(), password.end());

            // Add character set (default to UTF8)
            std::string charset = "UTF8";
            auto it = options.find("charset");
            if (it != options.end())
            {
                charset = it->second;
            }
            dpb.push_back(isc_dpb_lc_ctype);
            dpb.push_back(static_cast<char>(charset.length()));
            dpb.insert(dpb.end(), charset.begin(), charset.end());

            // Add role (optional, e.g. RDB$ADMIN)
            auto roleIt = options.find("role");
            if (roleIt != options.end() && !roleIt->second.empty())
            {
                const std::string &role = roleIt->second;
                dpb.push_back(isc_dpb_sql_role_name);
                dpb.push_back(static_cast<char>(role.length()));
                dpb.insert(dpb.end(), role.begin(), role.end());
            }

            // Allocate database handle
            isc_db_handle *dbHandle = new isc_db_handle(0);
            FIREBIRD_DEBUG("  Attaching to database...");

            // Attach to database
            if (isc_attach_database(status, 0, connStr.c_str(), dbHandle,
                                    static_cast<short>(dpb.size()), dpb.data()))
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Failed to attach: %s", errorMsg.c_str());
                delete dbHandle;
                m_initFailed = true;
                m_initError = DBException("FB7A8B9C0D1E", "Failed to connect to database: " + errorMsg,
                                          system_utils::captureCallStack());
                return;
            }
            FIREBIRD_DEBUG("  Attached successfully, dbHandle=%p, *dbHandle=%p", (void *)dbHandle, (void *)(uintptr_t)*dbHandle);

            // Create shared_ptr with custom deleter
            m_db = std::shared_ptr<isc_db_handle>(dbHandle, FirebirdDbDeleter{});

            // Cache URL — database already starts with '/' (prepended by parseURL for remote connections),
            // so no separator is added here to avoid a double slash (e.g. "localhost:3050//path").
            const std::string sep = (!database.empty() && database[0] == '/') ? "" : "/";
            m_url = "cpp_dbc:firebird://" + host + ":" + std::to_string(port) + sep + database;

            m_closed.store(false, std::memory_order_release);

            // Start initial transaction if autocommit is enabled
            FIREBIRD_DEBUG("  m_autoCommit: %s", (m_autoCommit ? "true" : "false"));
            if (m_autoCommit)
            {
                FIREBIRD_DEBUG("  Starting initial transaction...");
                auto txResult = startTransaction(std::nothrow);
                if (!txResult.has_value())
                {
                    // Clean up the database handle before reporting failure
                    m_db.reset();
                    m_initFailed = true;
                    m_initError = txResult.error();
                    return;
                }
            }
            FIREBIRD_DEBUG("FirebirdConnection::constructor - Done");
        }
        catch (const DBException &ex)
        {
            m_initFailed = true;
            m_initError = ex;
        }
        catch (const std::exception &ex)
        {
            m_initFailed = true;
            m_initError = DBException("NXZ242YS9FRK", ex.what(), system_utils::captureCallStack());
        }
        catch (...)
        {
            m_initFailed = true;
            m_initError = DBException("J24BZGLBC24P", "Unknown error in FirebirdDBConnection constructor",
                                      system_utils::captureCallStack());
        }
    }

    // ============================================================================
    // Private nothrow helpers
    // ============================================================================

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::registerStatement(std::nothrow_t, std::weak_ptr<FirebirdDBPreparedStatement> stmt) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("8M8M7HGIZ54V", "Connection closed");
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w)
                          { return w.expired(); });
        }
        m_activeStatements.insert(stmt);
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::unregisterStatement(std::nothrow_t, std::weak_ptr<FirebirdDBPreparedStatement> stmt) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("VHFDYJ7QU5JF", "Connection closed");
        m_activeStatements.erase(stmt);
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::registerResultSet(std::nothrow_t, std::weak_ptr<FirebirdDBResultSet> rs) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("Y5XZR2IJY743", "Connection closed");
        if (m_activeResultSets.size() > 50)
        {
            std::erase_if(m_activeResultSets, [](const auto &w)
                          { return w.expired(); });
        }
        m_activeResultSets.insert(rs);
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::unregisterResultSet(std::nothrow_t, std::weak_ptr<FirebirdDBResultSet> rs) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("N10W81OLXB29", "Connection closed");
        m_activeResultSets.erase(rs);
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::startTransaction(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::startTransaction(nothrow) - Starting");
        FIREBIRD_DEBUG("  m_tr: %ld", (long)m_tr);

        if (m_tr)
        {
            FIREBIRD_DEBUG("  Transaction already active, returning");
            return {}; // Transaction already active
        }

        ISC_STATUS_ARRAY status;

        // Build TPB (Transaction Parameter Block) based on isolation level
        std::vector<char> tpb;
        tpb.push_back(isc_tpb_version3);

        switch (m_isolationLevel)
        {
        case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
        {
            tpb.push_back(isc_tpb_read_committed);
            tpb.push_back(isc_tpb_rec_version);
            break;
        }
        case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
        {
            tpb.push_back(isc_tpb_read_committed);
            tpb.push_back(isc_tpb_no_rec_version);
            break;
        }
        case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
        {
            tpb.push_back(isc_tpb_concurrency);
            break;
        }
        case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
        {
            tpb.push_back(isc_tpb_consistency);
            break;
        }
        default:
        {
            tpb.push_back(isc_tpb_read_committed);
            tpb.push_back(isc_tpb_no_rec_version);
            break;
        }
        }

        tpb.push_back(isc_tpb_write);
        tpb.push_back(isc_tpb_wait);

        FIREBIRD_DEBUG("  Calling isc_start_transaction...");
        FIREBIRD_DEBUG("    m_db.get()=%p, *m_db.get()=%p", (void *)m_db.get(), (void *)(uintptr_t)(m_db.get() ? *m_db.get() : 0));
        if (isc_start_transaction(status, &m_tr, 1, m_db.get(),
                                  static_cast<unsigned short>(tpb.size()), tpb.data()))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to start transaction: %s", errorMsg.c_str());
            return cpp_dbc::unexpected(DBException("FB8B9C0D1E2F", "Failed to start transaction: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }

        m_transactionActive = true;
        FIREBIRD_DEBUG("FirebirdConnection::startTransaction(nothrow) - Done, m_tr=%ld", (long)m_tr);
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::endTransaction(std::nothrow_t, bool commit) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::endTransaction(nothrow) - Starting, commit=%s", (commit ? "true" : "false"));
        if (!m_tr)
        {
            FIREBIRD_DEBUG("  No active transaction (m_tr=0), returning");
            return {};
        }

        // CRITICAL: Close all active ResultSets BEFORE ending the transaction
        // In Firebird, statements are bound to transactions and must be freed
        // before the transaction ends, otherwise we get invalid memory access
        FIREBIRD_DEBUG("  Closing all active ResultSets before ending transaction");
        closeAllActiveResultSets(std::nothrow);

        ISC_STATUS_ARRAY status;

        if (commit)
        {
            FIREBIRD_DEBUG("  Calling isc_commit_transaction, m_tr=%ld", (long)m_tr);
            if (isc_commit_transaction(status, &m_tr))
            {
                FIREBIRD_DEBUG("  isc_commit_transaction failed: %s", interpretStatusVector(status).c_str());
                return cpp_dbc::unexpected(DBException("FB9C0D1E2F3A", "Failed to commit transaction: " + interpretStatusVector(status),
                                                       system_utils::captureCallStack()));
            }
            FIREBIRD_DEBUG("  isc_commit_transaction succeeded");
        }
        else
        {
            FIREBIRD_DEBUG("  Calling isc_rollback_transaction, m_tr=%ld", (long)m_tr);
            if (isc_rollback_transaction(status, &m_tr))
            {
                FIREBIRD_DEBUG("  isc_rollback_transaction failed: %s", interpretStatusVector(status).c_str());
                return cpp_dbc::unexpected(DBException("FB0D1E2F3A4B", "Failed to rollback transaction: " + interpretStatusVector(status),
                                                       system_utils::captureCallStack()));
            }
            FIREBIRD_DEBUG("  isc_rollback_transaction succeeded");
        }

        m_tr = 0;
        m_transactionActive = false;
        FIREBIRD_DEBUG("FirebirdConnection::endTransaction(nothrow) - Done");
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::closeAllActiveResultSets(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::closeAllActiveResultSets(nothrow) - Starting");

        // Collect result sets into a temporary vector while holding the lock,
        // then release the lock before calling close() to prevent:
        // 1. Iterator invalidation: close() -> unregisterResultSet() -> erase() on the iterated set
        // 2. Mutex hold during sleep_for(25ms) inside close()
        std::vector<std::shared_ptr<FirebirdDBResultSet>> resultSetsToClose;

        {
            FIREBIRD_CONNECTION_LOCK_OR_RETURN("2QTZY8G48KDG", "Connection closed");
            for (auto &weakRs : m_activeResultSets)
            {
                if (auto rs = weakRs.lock())
                {
                    resultSetsToClose.push_back(rs);
                }
            }
            m_activeResultSets.clear(); // Clear BEFORE releasing lock
        }

        // Now close result sets outside the lock
        // close() will call unregisterResultSet() which is now a no-op (set is already cleared)
        int closedCount = 0;
        for (auto &rs : resultSetsToClose)
        {
            auto closeResult = rs->close(std::nothrow);
            if (closeResult.has_value())
            {
                closedCount++;
            }
            else
            {
                FIREBIRD_DEBUG("  Failed to close ResultSet: %s", closeResult.error().what_s().data());
            }
        }
        FIREBIRD_DEBUG("FirebirdConnection::closeAllActiveResultSets(nothrow) - Closed %d result sets", closedCount);
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::closeAllActivePreparedStatements(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::closeAllActivePreparedStatements(nothrow) - Starting");

        // Collect statements into a temporary vector while holding the lock,
        // then release the lock before calling invalidate() to prevent deadlock.
        // This is necessary because invalidate() may call close() which could
        // re-enter the connection through unregisterStatement().
        std::vector<std::shared_ptr<FirebirdDBPreparedStatement>> statementsToInvalidate;

        {
            FIREBIRD_CONNECTION_LOCK_OR_RETURN("1KDSKXL120UP", "Connection closed");
            for (auto &weakStmt : m_activeStatements)
            {
                if (auto stmt = weakStmt.lock())
                {
                    statementsToInvalidate.push_back(stmt);
                }
            }
            m_activeStatements.clear();
        }

        // Now close statements outside the lock
        int closedCount = 0;
        for (auto &stmt : statementsToInvalidate)
        {
            auto closeResult = stmt->notifyConnClosing(std::nothrow);
            if (closeResult.has_value())
            {
                closedCount++;
            }
            else
            {
                FIREBIRD_DEBUG("  Failed to close PreparedStatement: %s", closeResult.error().what_s().data());
            }
        }

        FIREBIRD_DEBUG("FirebirdConnection::closeAllActivePreparedStatements(nothrow) - Closed %d prepared statements", closedCount);
        return {};
    }

    cpp_dbc::expected<uint64_t, DBException>
    FirebirdDBConnection::executeCreateDatabase(std::nothrow_t, const std::string &sql) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::executeCreateDatabase(nothrow) - Starting");
        FIREBIRD_DEBUG("  SQL: %s", sql.c_str());

        ISC_STATUS_ARRAY status;
        isc_db_handle db = 0;
        isc_tr_handle tr = 0;

        // Execute CREATE DATABASE using isc_dsql_execute_immediate
        // Note: For CREATE DATABASE, we pass null handles and the SQL creates the database
        if (isc_dsql_execute_immediate(status, &db, &tr, 0, sql.c_str(), SQL_DIALECT_V6, nullptr))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to create database or schema: %s", errorMsg.c_str());
            return cpp_dbc::unexpected(DBException("G8H4I0J6K2L8", "Failed to create database/schema: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }

        FIREBIRD_DEBUG("  Database created successfully!");

        // Detach from the newly created database
        if (db)
        {
            isc_detach_database(status, &db);
        }

        return static_cast<uint64_t>(0); // CREATE DATABASE doesn't return affected rows
    }

    FirebirdDBConnection::~FirebirdDBConnection()
    {
        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

    // ============================================================================
    // Throwing API — requires exception support
    // ============================================================================

#ifdef __cpp_exceptions

    void FirebirdDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool FirebirdDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool FirebirdDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBPreparedStatement> FirebirdDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBResultSet> FirebirdDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t FirebirdDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBConnection::setAutoCommit(bool autoCommit)
    {
        auto result = setAutoCommit(std::nothrow, autoCommit);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool FirebirdDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBConnection::transactionActive()
    {
        auto result = transactionActive(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel FirebirdDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::prepareForBorrow()
    {
        auto result = prepareForBorrow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
