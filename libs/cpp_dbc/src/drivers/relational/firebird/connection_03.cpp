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

 @file connection_03.cpp
 @brief Firebird database driver implementation - FirebirdDBConnection nothrow methods

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
    // FirebirdDBConnection - NOTHROW METHODS (part 2)
    // ============================================================================

    cpp_dbc::expected<uint64_t, DBException>
    FirebirdDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Starting");
            FIREBIRD_DEBUG("  SQL: " << sql);

            // Check if this is a CREATE DATABASE statement
            // CREATE DATABASE requires special handling with isc_dsql_execute_immediate
            std::string upperSql = sql;
            std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::toupper);

            // Remove leading whitespace for comparison
            size_t start = upperSql.find_first_not_of(" \t\n\r");
            if (start != std::string::npos)
            {
                upperSql = upperSql.substr(start);
            }

            if (upperSql.find("CREATE DATABASE") == 0 || upperSql.find("CREATE SCHEMA") == 0)
            {
                FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Detected CREATE DATABASE statement");
                try
                {
                    // Call executeCreateDatabase which can throw
                    return executeCreateDatabase(sql);
                }
                catch (const DBException &e)
                {
                    return cpp_dbc::unexpected(e);
                }
                catch (const std::exception &e)
                {
                    return cpp_dbc::unexpected(DBException("FC1A2B3C4D5E", std::string("Exception in executeCreateDatabase: ") + e.what(),
                                                           system_utils::captureCallStack()));
                }
            }

            // Check if this is a DDL statement that requires metadata lock cleanup
            // DDL operations like DROP, ALTER, CREATE, RECREATE need exclusive metadata locks
            // If there are active prepared statements holding metadata locks, we get deadlock
            bool isDDL = (upperSql.find("DROP ") == 0 ||
                          upperSql.find("ALTER ") == 0 ||
                          upperSql.find("CREATE ") == 0 ||
                          upperSql.find("RECREATE ") == 0);

            if (isDDL)
            {
                FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Detected DDL statement, cleaning up metadata locks");

                // Close all active prepared statements to release metadata locks
                closeAllActivePreparedStatements();

                // Commit current transaction to ensure all metadata locks are released
                if (m_tr)
                {
                    FIREBIRD_DEBUG("  Committing current transaction before DDL");
                    try
                    {
                        endTransaction(true);
                        startTransaction();
                    }
                    catch (const DBException &e)
                    {
                        FIREBIRD_DEBUG("  Commit before DDL failed: " << e.what());
                        return cpp_dbc::unexpected(e);
                    }
                }

                FIREBIRD_DEBUG("  Metadata locks cleanup completed");
            }

            // First, prepare the statement using the nothrow version
            auto stmtResult = prepareStatement(std::nothrow, sql);
            if (!stmtResult)
            {
                return cpp_dbc::unexpected(stmtResult.error());
            }

            // Now execute the update using the nothrow version
            auto stmt = stmtResult.value();
            auto updateResult = stmt->executeUpdate(std::nothrow);
            if (!updateResult)
            {
                return cpp_dbc::unexpected(updateResult.error());
            }

            FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Done");
            return updateResult.value();
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C3D4E5F6A7B8", std::string("Exception in executeUpdate: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D4E5F6A7B8C9", "Unknown exception in executeUpdate",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            FIREBIRD_DEBUG("FirebirdConnection::setAutoCommit(nothrow) - Starting");
            FIREBIRD_DEBUG("  Current autoCommit: " << (m_autoCommit ? "true" : "false"));
            FIREBIRD_DEBUG("  New autoCommit: " << (autoCommit ? "true" : "false"));

            if (m_autoCommit == autoCommit)
            {
                FIREBIRD_DEBUG("  No change needed, returning");
                return {};
            }

            if (m_autoCommit && !autoCommit)
            {
                // Switching from auto-commit to manual
                // Commit any pending transaction
                if (m_tr)
                {
                    FIREBIRD_DEBUG("  Switching from auto-commit to manual, committing pending transaction");
                    auto commitResult = commit(std::nothrow);
                    if (!commitResult)
                    {
                        return cpp_dbc::unexpected(commitResult.error());
                    }
                }
            }

            m_autoCommit = autoCommit;
            FIREBIRD_DEBUG("  AutoCommit set to: " << (m_autoCommit ? "true" : "false"));

            if (m_autoCommit && !m_tr)
            {
                FIREBIRD_DEBUG("  AutoCommit enabled but no transaction, starting one...");
                try
                {
                    // Use the throwing version for now until we implement the nothrow version
                    startTransaction();
                }
                catch (const DBException &e)
                {
                    FIREBIRD_DEBUG("  startTransaction failed: " << e.what());
                    return cpp_dbc::unexpected(e);
                }
            }

            FIREBIRD_DEBUG("FirebirdConnection::setAutoCommit(nothrow) - Done");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("FB7S9T0U1V2W", std::string("Exception in setAutoCommit: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("FB8T0U1V2W3X", "Unknown exception in setAutoCommit",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            FIREBIRD_DEBUG("FirebirdConnection::getAutoCommit(nothrow) - Returning " << (m_autoCommit ? "true" : "false"));
            return m_autoCommit;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("FB9U1V2W3X4Y", std::string("Exception in getAutoCommit: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B8C9D0E1F2A3", "Unknown exception in getAutoCommit",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_DEBUG("FirebirdConnection::beginTransaction(nothrow) - Starting");
            FIREBIRD_DEBUG("  m_autoCommit before: " << (m_autoCommit ? "true" : "false"));
            FIREBIRD_DEBUG("  m_transactionActive: " << (m_transactionActive ? "true" : "false"));

            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            // Disable autocommit when beginning a manual transaction
            // This prevents executeUpdate from auto-committing
            m_autoCommit = false;
            FIREBIRD_DEBUG("  m_autoCommit after: " << (m_autoCommit ? "true" : "false"));

            // If transaction is already active, just return true (like MySQL)
            if (m_transactionActive)
            {
                FIREBIRD_DEBUG("FirebirdConnection::beginTransaction(nothrow) - Transaction already active, returning true");
                return true;
            }

            try
            {
                startTransaction();
                FIREBIRD_DEBUG("FirebirdConnection::beginTransaction(nothrow) - Done");
                return true;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected(e);
            }
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C9D0E1F2A3B4", std::string("Exception in beginTransaction: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D0E1F2A3B4C5", "Unknown exception in beginTransaction",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            FIREBIRD_DEBUG("FirebirdConnection::transactionActive(nothrow) - Returning " << (m_transactionActive ? "true" : "false"));
            return m_transactionActive;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E1F2A3B4C5D6", std::string("Exception in transactionActive: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F2A3B4C5D6E7", "Unknown exception in transactionActive",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::commit(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_DEBUG("FirebirdConnection::commit(nothrow) - Starting");

            FIREBIRD_DEBUG("  Acquiring lock...");
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
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

            FIREBIRD_DEBUG("FirebirdConnection::commit(nothrow) - Done");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A3B4C5D6E7F8", std::string("Exception in commit: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B4C5D6E7F8A9", "Unknown exception in commit",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::rollback(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_DEBUG("FirebirdConnection::rollback(nothrow) - Starting");

            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            FIREBIRD_DEBUG("  Calling endTransaction(false)...");
            endTransaction(false);
            FIREBIRD_DEBUG("  endTransaction completed");

            if (m_autoCommit)
            {
                FIREBIRD_DEBUG("  AutoCommit is enabled, calling startTransaction()...");
                startTransaction();
                FIREBIRD_DEBUG("  startTransaction completed");
            }

            FIREBIRD_DEBUG("FirebirdConnection::rollback(nothrow) - Done");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C5D6E7F8A9B0", std::string("Exception in rollback: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D6E7F8A9B0C1", "Unknown exception in rollback",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        try
        {
            FIREBIRD_DEBUG("FirebirdConnection::setTransactionIsolation(nothrow) - Starting");
            FIREBIRD_DEBUG("  Current level: " << static_cast<int>(m_isolationLevel));
            FIREBIRD_DEBUG("  New level: " << static_cast<int>(level));

            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            // If the isolation level is already set to the requested level, do nothing
            if (m_isolationLevel == level)
            {
                FIREBIRD_DEBUG("  No change needed, returning");
                return {};
            }

            // If a transaction is active, we need to end it first, change the isolation level,
            // and restart the transaction with the new isolation level
            bool hadActiveTransaction = m_transactionActive;
            if (m_transactionActive)
            {
                FIREBIRD_DEBUG("  Transaction is active, ending it first");
                // Commit the current transaction (or rollback if autocommit is off)
                if (m_autoCommit)
                {
                    FIREBIRD_DEBUG("  AutoCommit is enabled, committing current transaction");
                    endTransaction(true); // Commit
                }
                else
                {
                    FIREBIRD_DEBUG("  AutoCommit is disabled, rolling back current transaction");
                    endTransaction(false); // Rollback
                }
            }

            m_isolationLevel = level;
            FIREBIRD_DEBUG("  Isolation level set to: " << static_cast<int>(m_isolationLevel));

            // Restart the transaction if we had one active and autocommit is on
            if (hadActiveTransaction && m_autoCommit)
            {
                FIREBIRD_DEBUG("  Restarting transaction with new isolation level");
                try
                {
                    startTransaction();
                }
                catch (const DBException &e)
                {
                    FIREBIRD_DEBUG("  Failed to restart transaction: " << e.what());
                    return cpp_dbc::unexpected(e);
                }
            }

            FIREBIRD_DEBUG("FirebirdConnection::setTransactionIsolation(nothrow) - Done");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E7F8A9B0C1D2", std::string("Exception in setTransactionIsolation: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F8A9B0C1D2E3", "Unknown exception in setTransactionIsolation",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    FirebirdDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            FIREBIRD_DEBUG("FirebirdConnection::getTransactionIsolation(nothrow) - Returning " << static_cast<int>(m_isolationLevel));
            return m_isolationLevel;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("G9A0B1C2D3E4", std::string("Exception in getTransactionIsolation: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("H0B1C2D3E4F5", "Unknown exception in getTransactionIsolation",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
