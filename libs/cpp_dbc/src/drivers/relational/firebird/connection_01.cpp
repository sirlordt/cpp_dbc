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
 @brief Firebird database driver implementation - FirebirdDBConnection throwing methods (part 1)

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

    FirebirdDBConnection::FirebirdDBConnection(const std::string &host,
                                               int port,
                                               const std::string &database,
                                               const std::string &user,
                                               const std::string &password,
                                               const std::map<std::string, std::string> &options)
        : m_tr(0), m_isolationLevel(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED)
#if DB_DRIVER_THREAD_SAFE
        , m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
    {
        FIREBIRD_DEBUG("FirebirdConnection::constructor - Starting");
        FIREBIRD_DEBUG("  host: " << host);
        FIREBIRD_DEBUG("  port: " << port);
        FIREBIRD_DEBUG("  database: " << database);
        FIREBIRD_DEBUG("  user: " << user);

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
        FIREBIRD_DEBUG("  Connection string: " << connStr);

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

        // Allocate database handle
        isc_db_handle *dbHandle = new isc_db_handle(0);
        FIREBIRD_DEBUG("  Attaching to database...");

        // Attach to database
        if (isc_attach_database(status, 0, connStr.c_str(), dbHandle,
                                static_cast<short>(dpb.size()), dpb.data()))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to attach: " << errorMsg);
            delete dbHandle;
            throw DBException("FB7A8B9C0D1E", "Failed to connect to database: " + errorMsg,
                              system_utils::captureCallStack());
        }
        FIREBIRD_DEBUG("  Attached successfully, dbHandle=" << dbHandle << ", *dbHandle=" << *dbHandle);

        // Create shared_ptr with custom deleter
        m_db = std::shared_ptr<isc_db_handle>(dbHandle, FirebirdDbDeleter{});

        // Cache URL
        m_url = "cpp_dbc:firebird://" + host + ":" + std::to_string(port) + "/" + database;

        m_closed = false;

        // Start initial transaction if autocommit is enabled
        FIREBIRD_DEBUG("  m_autoCommit: " << (m_autoCommit ? "true" : "false"));
        if (m_autoCommit)
        {
            FIREBIRD_DEBUG("  Starting initial transaction...");
            startTransaction();
        }
        FIREBIRD_DEBUG("FirebirdConnection::constructor - Done");
    }

    FirebirdDBConnection::~FirebirdDBConnection()
    {
        close();
    }

    void FirebirdDBConnection::registerStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt)
    {
        std::lock_guard<std::mutex> lock(m_statementsMutex);
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w) { return w.expired(); });
        }
        m_activeStatements.insert(stmt);
    }

    void FirebirdDBConnection::unregisterStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt)
    {
        std::lock_guard<std::mutex> lock(m_statementsMutex);
        m_activeStatements.erase(stmt);
    }

    void FirebirdDBConnection::registerResultSet(std::weak_ptr<FirebirdDBResultSet> rs)
    {
        std::lock_guard<std::mutex> lock(m_resultSetsMutex);
        if (m_activeResultSets.size() > 50)
        {
            std::erase_if(m_activeResultSets, [](const auto &w) { return w.expired(); });
        }
        m_activeResultSets.insert(rs);
    }

    void FirebirdDBConnection::unregisterResultSet(std::weak_ptr<FirebirdDBResultSet> rs)
    {
        std::lock_guard<std::mutex> lock(m_resultSetsMutex);
        m_activeResultSets.erase(rs);
    }

    void FirebirdDBConnection::startTransaction()
    {
        FIREBIRD_DEBUG("FirebirdConnection::startTransaction - Starting");
        FIREBIRD_DEBUG("  m_tr: " << m_tr);

        if (m_tr)
        {
            FIREBIRD_DEBUG("  Transaction already active, returning");
            return; // Transaction already active
        }

        ISC_STATUS_ARRAY status;

        // Build TPB (Transaction Parameter Block) based on isolation level
        std::vector<char> tpb;
        tpb.push_back(isc_tpb_version3);

        switch (m_isolationLevel)
        {
        case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
            tpb.push_back(isc_tpb_read_committed);
            tpb.push_back(isc_tpb_rec_version);
            break;
        case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
            tpb.push_back(isc_tpb_read_committed);
            tpb.push_back(isc_tpb_no_rec_version);
            break;
        case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
            tpb.push_back(isc_tpb_concurrency);
            break;
        case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
            tpb.push_back(isc_tpb_consistency);
            break;
        default:
            tpb.push_back(isc_tpb_read_committed);
            tpb.push_back(isc_tpb_no_rec_version);
            break;
        }

        tpb.push_back(isc_tpb_write);
        tpb.push_back(isc_tpb_wait);

        FIREBIRD_DEBUG("  Calling isc_start_transaction...");
        FIREBIRD_DEBUG("    m_db.get()=" << m_db.get() << ", *m_db.get()=" << (m_db.get() ? *m_db.get() : 0));
        if (isc_start_transaction(status, &m_tr, 1, m_db.get(),
                                  static_cast<unsigned short>(tpb.size()), tpb.data()))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to start transaction: " << errorMsg);
            throw DBException("FB8B9C0D1E2F", "Failed to start transaction: " + errorMsg,
                              system_utils::captureCallStack());
        }

        m_transactionActive = true;
        FIREBIRD_DEBUG("FirebirdConnection::startTransaction - Done, m_tr=" << m_tr);
    }

    void FirebirdDBConnection::endTransaction(bool commit)
    {
        FIREBIRD_DEBUG("FirebirdConnection::endTransaction - Starting, commit=" << (commit ? "true" : "false"));
        if (!m_tr)
        {
            FIREBIRD_DEBUG("  No active transaction (m_tr=0), returning");
            return;
        }

        // CRITICAL: Close all active ResultSets BEFORE ending the transaction
        // In Firebird, statements are bound to transactions and must be freed
        // before the transaction ends, otherwise we get invalid memory access
        FIREBIRD_DEBUG("  Closing all active ResultSets before ending transaction");
        closeAllActiveResultSets();

        ISC_STATUS_ARRAY status;

        if (commit)
        {
            FIREBIRD_DEBUG("  Calling isc_commit_transaction, m_tr=" << m_tr);
            if (isc_commit_transaction(status, &m_tr))
            {
                FIREBIRD_DEBUG("  isc_commit_transaction failed: " << interpretStatusVector(status));
                throw DBException("FB9C0D1E2F3A", "Failed to commit transaction: " + interpretStatusVector(status),
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  isc_commit_transaction succeeded");
        }
        else
        {
            FIREBIRD_DEBUG("  Calling isc_rollback_transaction, m_tr=" << m_tr);
            if (isc_rollback_transaction(status, &m_tr))
            {
                FIREBIRD_DEBUG("  isc_rollback_transaction failed: " << interpretStatusVector(status));
                throw DBException("FB0D1E2F3A4B", "Failed to rollback transaction: " + interpretStatusVector(status),
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  isc_rollback_transaction succeeded");
        }

        m_tr = 0;
        m_transactionActive = false;
        FIREBIRD_DEBUG("FirebirdConnection::endTransaction - Done");
    }

    void FirebirdDBConnection::closeAllActiveResultSets()
    {
        FIREBIRD_DEBUG("FirebirdConnection::closeAllActiveResultSets - Starting");
        std::lock_guard<std::mutex> lock(m_resultSetsMutex);

        int closedCount = 0;
        for (auto &weakRs : m_activeResultSets)
        {
            if (auto rs = weakRs.lock())
            {
                try
                {
                    rs->close();
                    closedCount++;
                }
                catch (...)
                {
                    FIREBIRD_DEBUG("  Exception while closing ResultSet, ignoring");
                }
            }
        }
        m_activeResultSets.clear();
        FIREBIRD_DEBUG("FirebirdConnection::closeAllActiveResultSets - Closed " << closedCount << " result sets");
    }

    void FirebirdDBConnection::closeAllActivePreparedStatements()
    {
        FIREBIRD_DEBUG("FirebirdConnection::closeAllActivePreparedStatements - Starting");

        // Collect statements into a temporary vector while holding the lock,
        // then release the lock before calling invalidate() to prevent deadlock.
        // This is necessary because invalidate() may call close() which could
        // re-enter the connection through unregisterStatement().
        std::vector<std::shared_ptr<FirebirdDBPreparedStatement>> statementsToInvalidate;

        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            for (auto &weakStmt : m_activeStatements)
            {
                if (auto stmt = weakStmt.lock())
                {
                    statementsToInvalidate.push_back(stmt);
                }
            }
            m_activeStatements.clear();
        }

        // Now invalidate statements outside the lock
        int invalidatedCount = 0;
        for (auto &stmt : statementsToInvalidate)
        {
            try
            {
                stmt->invalidate();
                invalidatedCount++;
            }
            catch (...)
            {
                FIREBIRD_DEBUG("  Exception while invalidating PreparedStatement, ignoring");
            }
        }

        FIREBIRD_DEBUG("FirebirdConnection::closeAllActivePreparedStatements - Invalidated " << invalidatedCount << " prepared statements");
    }

    void FirebirdDBConnection::close()
    {

        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed)
            return;

        // Notify all active statements
        {
            std::lock_guard<std::mutex> stmtLock(m_statementsMutex);
            for (auto &weakStmt : m_activeStatements)
            {
                if (auto stmt = weakStmt.lock())
                {
                    stmt->notifyConnClosing();
                }
            }
            m_activeStatements.clear();
        }

        // End any active transaction
        if (m_tr)
        {
            ISC_STATUS_ARRAY status;
            isc_rollback_transaction(status, &m_tr);
            m_tr = 0;
        }

        // The database handle will be closed by the shared_ptr deleter
        m_db.reset();

        m_closed = true;

        // Small delay to ensure cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    bool FirebirdDBConnection::isClosed() const
    {

        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        return m_closed;
    }

    void FirebirdDBConnection::returnToPool()
    {
        FIREBIRD_DEBUG("FirebirdConnection::returnToPool - Starting");
        FIREBIRD_DEBUG("  m_transactionActive: " << (m_transactionActive ? "true" : "false"));
        FIREBIRD_DEBUG("  m_autoCommit: " << (m_autoCommit ? "true" : "false"));
        FIREBIRD_DEBUG("  m_tr: " << m_tr);

        // NOTE: We don't call closeAllActiveResultSets() here because it's already
        // called in endTransaction() which will be invoked by commit()/rollback()
        // Calling it twice causes "invalid statement handle" errors

        // Reset connection state for pool reuse
        // Always ensure we have a clean transaction state
        if (m_tr)
        {
            FIREBIRD_DEBUG("  Transaction handle exists, committing/rolling back");
            try
            {
                if (m_autoCommit)
                {
                    // In autocommit mode, commit any pending changes
                    // This will call endTransaction() which closes ResultSets
                    commit();
                }
                else if (m_transactionActive)
                {
                    // In manual mode with active transaction, rollback
                    // This will call endTransaction() which closes ResultSets
                    rollback();
                }
            }
            catch (...)
            {
                FIREBIRD_DEBUG("  Commit/rollback failed, forcing rollback");
                // If commit/rollback fails, force a rollback to clean up
                try
                {
                    ISC_STATUS_ARRAY status;
                    if (m_tr)
                    {
                        isc_rollback_transaction(status, &m_tr);
                        m_tr = 0;
                    }
                }
                catch (...)
                {
                    FIREBIRD_DEBUG("  Force rollback also failed");
                }
            }
        }

        // Ensure autocommit is enabled for pool reuse (default state)
        m_autoCommit = true;
        m_transactionActive = false;

        // Start a fresh transaction for the next use
        if (!m_tr && !m_closed)
        {
            FIREBIRD_DEBUG("  Starting fresh transaction for pool reuse");
            try
            {
                startTransaction();
            }
            catch (...)
            {
                FIREBIRD_DEBUG("  Failed to start fresh transaction");
            }
        }

        FIREBIRD_DEBUG("FirebirdConnection::returnToPool - Done, m_tr=" << m_tr);
    }

    bool FirebirdDBConnection::isPooled()
    {
        return false; // Not pooled by default
    }

    std::shared_ptr<RelationalDBPreparedStatement> FirebirdDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBResultSet> FirebirdDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t FirebirdDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBConnection::setAutoCommit(bool autoCommit)
    {
        auto result = setAutoCommit(std::nothrow, autoCommit);
        if (!result)
        {
            throw result.error();
        }
    }

    bool FirebirdDBConnection::getAutoCommit()
    {

        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        return m_autoCommit;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
