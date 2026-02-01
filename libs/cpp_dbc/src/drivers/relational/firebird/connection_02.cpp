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

 @file connection_02.cpp
 @brief Firebird database driver implementation - FirebirdDBConnection throwing methods (part 2)

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

    bool FirebirdDBConnection::beginTransaction()
    {
        FIREBIRD_DEBUG("FirebirdConnection::beginTransaction - Starting");
        FIREBIRD_DEBUG("  m_autoCommit before: " << (m_autoCommit ? "true" : "false"));
        FIREBIRD_DEBUG("  m_transactionActive: " << (m_transactionActive ? "true" : "false"));

        DB_DRIVER_LOCK_GUARD(m_connMutex);

        // Disable autocommit when beginning a manual transaction
        // This prevents executeUpdate from auto-committing
        // IMPORTANT: Must be done BEFORE checking m_transactionActive
        // because in Firebird, a transaction is always active (started in constructor)
        m_autoCommit = false;
        FIREBIRD_DEBUG("  m_autoCommit after: " << (m_autoCommit ? "true" : "false"));

        // If transaction is already active, just return true (like MySQL)
        if (m_transactionActive)
        {
            FIREBIRD_DEBUG("FirebirdConnection::beginTransaction - Transaction already active, returning true");
            return true;
        }

        startTransaction();
        FIREBIRD_DEBUG("FirebirdConnection::beginTransaction - Done");
        return true;
    }

    bool FirebirdDBConnection::transactionActive()
    {

        DB_DRIVER_LOCK_GUARD(m_connMutex);

        return m_transactionActive;
    }

    void FirebirdDBConnection::commit()
    {
        FIREBIRD_DEBUG("FirebirdConnection::commit - Starting");

        FIREBIRD_DEBUG("  Acquiring lock...");
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        FIREBIRD_DEBUG("  Lock acquired");

        FIREBIRD_DEBUG("  Calling endTransaction(true)...");
        endTransaction(true);
        FIREBIRD_DEBUG("  endTransaction completed");

        if (m_autoCommit)
        {
            FIREBIRD_DEBUG("  AutoCommit is enabled, calling startTransaction()...");
            startTransaction();
            FIREBIRD_DEBUG("  startTransaction completed");
        }
        FIREBIRD_DEBUG("FirebirdConnection::commit - Done");
    }

    void FirebirdDBConnection::rollback()
    {

        DB_DRIVER_LOCK_GUARD(m_connMutex);

        endTransaction(false);

        if (m_autoCommit)
        {
            startTransaction();
        }
    }

    void FirebirdDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {

        DB_DRIVER_LOCK_GUARD(m_connMutex);

        // If the isolation level is already set to the requested level, do nothing
        if (m_isolationLevel == level)
        {
            return;
        }

        // If a transaction is active, we need to end it first, change the isolation level,
        // and restart the transaction with the new isolation level
        bool hadActiveTransaction = m_transactionActive;
        if (m_transactionActive)
        {
            // Commit the current transaction (or rollback if autocommit is off)
            if (m_autoCommit)
            {
                endTransaction(true); // Commit
            }
            else
            {
                endTransaction(false); // Rollback
            }
        }

        m_isolationLevel = level;

        // Restart the transaction if we had one active and autocommit is on
        if (hadActiveTransaction && m_autoCommit)
        {
            startTransaction();
        }
    }

    TransactionIsolationLevel FirebirdDBConnection::getTransactionIsolation()
    {

        DB_DRIVER_LOCK_GUARD(m_connMutex);

        return m_isolationLevel;
    }

    std::string FirebirdDBConnection::getURL() const
    {
        return m_url;
    }

    uint64_t FirebirdDBConnection::executeCreateDatabase(const std::string &sql)
    {
        FIREBIRD_DEBUG("FirebirdConnection::executeCreateDatabase - Starting");
        FIREBIRD_DEBUG("  SQL: " << sql);

        ISC_STATUS_ARRAY status;
        isc_db_handle db = 0;
        isc_tr_handle tr = 0;

        // Execute CREATE DATABASE using isc_dsql_execute_immediate
        // Note: For CREATE DATABASE, we pass null handles and the SQL creates the database
        if (isc_dsql_execute_immediate(status, &db, &tr, 0, sql.c_str(), SQL_DIALECT_V6, nullptr))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to create database or schema: " << errorMsg);
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

    // ============================================================================
    // FirebirdDBConnection - NOTHROW METHODS (part 1)
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
    FirebirdDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_connMutex);

            FIREBIRD_DEBUG("FirebirdConnection::prepareStatement(nothrow) - Starting");
            FIREBIRD_DEBUG("  SQL: " << sql);
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_tr: " << m_tr);

            if (m_closed)
            {
                FIREBIRD_DEBUG("  Connection is closed!");
                return cpp_dbc::unexpected(DBException("C5DB7C0E1EE3", "Connection is closed", system_utils::captureCallStack()));
            }

            if (!m_tr)
            {
                FIREBIRD_DEBUG("  No active transaction, starting one...");
                auto startTrResult = beginTransaction(std::nothrow);
                if (!startTrResult)
                {
                    return cpp_dbc::unexpected(startTrResult.error());
                }
            }

            FIREBIRD_DEBUG("  Creating FirebirdDBPreparedStatement...");
            FIREBIRD_DEBUG("    m_db.get()=" << m_db.get() << ", *m_db.get()=" << (m_db.get() ? *m_db.get() : 0));
            FIREBIRD_DEBUG("    &m_tr=" << &m_tr << ", m_tr=" << m_tr);
            auto stmt = std::make_shared<FirebirdDBPreparedStatement>(
                std::weak_ptr<isc_db_handle>(m_db), &m_tr, sql, shared_from_this());

            FIREBIRD_DEBUG("FirebirdConnection::prepareStatement(nothrow) - Done");
            return cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>(std::static_pointer_cast<RelationalDBPreparedStatement>(stmt));
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("2791E5F8FC3C", std::string("Exception in prepareStatement: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D87AA3FA1250", "Unknown exception in prepareStatement", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
    FirebirdDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_connMutex);

            FIREBIRD_DEBUG("FirebirdConnection::executeQuery(nothrow) - Starting");
            FIREBIRD_DEBUG("  SQL: " << sql);

            // First, prepare the statement using the nothrow version
            auto stmtResult = prepareStatement(std::nothrow, sql);
            if (!stmtResult)
            {
                return cpp_dbc::unexpected(stmtResult.error());
            }

            // Now execute the query using the nothrow version
            auto stmt = stmtResult.value();
            auto resultSetResult = stmt->executeQuery(std::nothrow);
            if (!resultSetResult)
            {
                return cpp_dbc::unexpected(resultSetResult.error());
            }

            FIREBIRD_DEBUG("FirebirdConnection::executeQuery(nothrow) - Done");
            return resultSetResult.value();
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("F1E2D3C4B5A6", std::string("Exception in executeQuery: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("FBDY5Z6A7B8C", "Unknown exception in executeQuery", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
