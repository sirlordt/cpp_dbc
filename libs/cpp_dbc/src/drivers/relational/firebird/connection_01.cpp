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
          ,
          m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
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
            throw DBException("FB7A8B9C0D1E", "Failed to connect to database: " + errorMsg,
                              system_utils::captureCallStack());
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
            startTransaction();
        }
        FIREBIRD_DEBUG("FirebirdConnection::constructor - Done");
    }

    FirebirdDBConnection::~FirebirdDBConnection()
    {
        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

    void FirebirdDBConnection::registerStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt)
    {
        FIREBIRD_CONNECTION_LOCK_OR_THROW("8M8M7HGIZ54V", "Connection closed");
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w)
                          { return w.expired(); });
        }
        m_activeStatements.insert(stmt);
    }

    void FirebirdDBConnection::unregisterStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt)
    {
        FIREBIRD_CONNECTION_LOCK_OR_THROW("VHFDYJ7QU5JF", "Connection closed");
        m_activeStatements.erase(stmt);
    }

    void FirebirdDBConnection::registerResultSet(std::weak_ptr<FirebirdDBResultSet> rs)
    {
        FIREBIRD_CONNECTION_LOCK_OR_THROW("Y5XZR2IJY743", "Connection closed");
        if (m_activeResultSets.size() > 50)
        {
            std::erase_if(m_activeResultSets, [](const auto &w)
                          { return w.expired(); });
        }
        m_activeResultSets.insert(rs);
    }

    void FirebirdDBConnection::unregisterResultSet(std::weak_ptr<FirebirdDBResultSet> rs)
    {
        FIREBIRD_CONNECTION_LOCK_OR_THROW("N10W81OLXB29", "Connection closed");
        m_activeResultSets.erase(rs);
    }

    void FirebirdDBConnection::startTransaction()
    {
        FIREBIRD_DEBUG("FirebirdConnection::startTransaction - Starting");
        FIREBIRD_DEBUG("  m_tr: %ld", (long)m_tr);

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
        FIREBIRD_DEBUG("    m_db.get()=%p, *m_db.get()=%p", (void *)m_db.get(), (void *)(uintptr_t)(m_db.get() ? *m_db.get() : 0));
        if (isc_start_transaction(status, &m_tr, 1, m_db.get(),
                                  static_cast<unsigned short>(tpb.size()), tpb.data()))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to start transaction: %s", errorMsg.c_str());
            throw DBException("FB8B9C0D1E2F", "Failed to start transaction: " + errorMsg,
                              system_utils::captureCallStack());
        }

        m_transactionActive = true;
        FIREBIRD_DEBUG("FirebirdConnection::startTransaction - Done, m_tr=%ld", (long)m_tr);
    }

    void FirebirdDBConnection::endTransaction(bool commit)
    {
        FIREBIRD_DEBUG("FirebirdConnection::endTransaction - Starting, commit=%s", (commit ? "true" : "false"));
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
            FIREBIRD_DEBUG("  Calling isc_commit_transaction, m_tr=%ld", (long)m_tr);
            if (isc_commit_transaction(status, &m_tr))
            {
                FIREBIRD_DEBUG("  isc_commit_transaction failed: %s", interpretStatusVector(status).c_str());
                throw DBException("FB9C0D1E2F3A", "Failed to commit transaction: " + interpretStatusVector(status),
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  isc_commit_transaction succeeded");
        }
        else
        {
            FIREBIRD_DEBUG("  Calling isc_rollback_transaction, m_tr=%ld", (long)m_tr);
            if (isc_rollback_transaction(status, &m_tr))
            {
                FIREBIRD_DEBUG("  isc_rollback_transaction failed: %s", interpretStatusVector(status).c_str());
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

        // Collect result sets into a temporary vector while holding the lock,
        // then release the lock before calling close() to prevent:
        // 1. Iterator invalidation: close() -> unregisterResultSet() -> erase() on the iterated set
        // 2. Mutex hold during sleep_for(25ms) inside close()
        std::vector<std::shared_ptr<FirebirdDBResultSet>> resultSetsToClose;

        {
            FIREBIRD_CONNECTION_LOCK_OR_THROW("2QTZY8G48KDG", "Connection closed");
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
        FIREBIRD_DEBUG("FirebirdConnection::closeAllActiveResultSets - Closed %d result sets", closedCount);
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
            FIREBIRD_CONNECTION_LOCK_OR_THROW("1KDSKXL120UP", "Connection closed");
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

        FIREBIRD_DEBUG("FirebirdConnection::closeAllActivePreparedStatements - Invalidated %d prepared statements", invalidatedCount);
    }

    uint64_t FirebirdDBConnection::executeCreateDatabase(const std::string &sql)
    {
        FIREBIRD_DEBUG("FirebirdConnection::executeCreateDatabase - Starting");
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
            throw DBException("G8H4I0J6K2L8", "Failed to create database/schema: " + errorMsg,
                              system_utils::captureCallStack());
        }

        FIREBIRD_DEBUG("  Database created successfully!");

        // Detach from the newly created database
        if (db)
        {
            isc_detach_database(status, &db);
        }

        return 0; // CREATE DATABASE doesn't return affected rows
    }

    bool FirebirdDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBConnection::ping(std::nothrow_t) noexcept
    {
        auto result = executeQuery(std::nothrow, "SELECT 1 FROM RDB$DATABASE");
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        auto closeResult = result.value()->close(std::nothrow);
        if (!closeResult.has_value())
        {
            return cpp_dbc::unexpected(closeResult.error());
        }
        return true;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
