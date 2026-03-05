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

 @file connection_02.cpp
 @brief SQLite database driver implementation - SQLiteDBConnection nothrow methods

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <climits> // Para INT_MAX
#include <cstdlib> // Para getenv
#include <fstream> // Para std::ifstream
#include <charconv>
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // Nothrow API
    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> SQLiteDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("R0Z1A2B3C4D5", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            auto stmtResult = SQLiteDBPreparedStatement::create(std::nothrow, std::weak_ptr<sqlite3>(m_db), weak_from_this(), m_globalFileMutex, sql);
            if (!stmtResult.has_value())
            {
                return cpp_dbc::unexpected(stmtResult.error());
            }
            auto stmt = stmtResult.value();
            [[maybe_unused]] auto regResult = registerStatement(std::nothrow, std::weak_ptr<SQLiteDBPreparedStatement>(stmt));
            return std::shared_ptr<RelationalDBPreparedStatement>(stmt);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("PS1A1B2C3D4E",
                                                   std::string("prepareStatement failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("PS1A1B2C3D4F",
                                                   "prepareStatement failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> SQLiteDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("R1Z2A3B4C5D6", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            sqlite3_stmt *stmt = nullptr;
            int result = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                std::string errorMsg = sqlite3_errmsg(m_db.get());
                return cpp_dbc::unexpected(DBException("R2Z3A4B5C6D7", "Failed to prepare query: " + errorMsg,
                                                       system_utils::captureCallStack()));
                // std::vector<cpp_dbc::system_utils::StackFrame>()));
            }

            if (!stmt)
            {
                return cpp_dbc::unexpected(DBException("1DEA86F65A95", "Statement is null after successful preparation",
                                                       system_utils::captureCallStack()));
            }

            auto self = std::dynamic_pointer_cast<SQLiteDBConnection>(shared_from_this());
            // Pass global file mutex to ResultSet - required because SQLite uses cursor-based iteration
            // where sqlite3_step() and sqlite3_column_*() access the connection handle on every call.
            // Unlike MySQL/PostgreSQL where results are fully loaded into client memory.
            auto rsResult = SQLiteDBResultSet::create(std::nothrow, stmt, true, self, nullptr, m_globalFileMutex);
            if (!rsResult.has_value())
            {
                return cpp_dbc::unexpected(rsResult.error());
            }
            return std::shared_ptr<RelationalDBResultSet>(rsResult.value());
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("EQ2A1B2C3D4E",
                                                   std::string("executeQuery failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("EQ2A1B2C3D4F",
                                                   "executeQuery failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> SQLiteDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("R3Z4A5B6C7D8", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            char *errmsg = nullptr;
            int result = sqlite3_exec(m_db.get(), sql.c_str(), nullptr, nullptr, &errmsg);

            if (result != SQLITE_OK)
            {
                std::string error = errmsg ? errmsg : "Unknown error";
                sqlite3_free(errmsg);
                return cpp_dbc::unexpected(DBException("R4Z5A6B7C8D9", "Failed to execute update: " + error,
                                                       system_utils::captureCallStack()));
            }

            return static_cast<uint64_t>(sqlite3_changes(m_db.get()));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("EU2A1B2C3D4E",
                                                   std::string("executeUpdate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("EU2A1B2C3D4F",
                                                   "executeUpdate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("R5Z6A7B8C9D0", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            // Only change the state if we're actually changing the mode
            if (this->m_autoCommit.load(std::memory_order_acquire) != autoCommit)
            {
                if (autoCommit)
                {
                    // Enabling autocommit - commit any active transaction first
                    if (m_transactionActive.load(std::memory_order_acquire))
                    {
                        auto commitResult = commit(std::nothrow);
                        if (!commitResult)
                        {
                            return cpp_dbc::unexpected(commitResult.error());
                        }
                    }

                    this->m_autoCommit.store(true, std::memory_order_release);
                    this->m_transactionActive.store(false, std::memory_order_release);
                }
                else
                {
                    // Disabling autocommit - begin a transaction automatically (like MySQL driver)
                    auto beginResult = beginTransaction(std::nothrow);
                    if (!beginResult)
                    {
                        return cpp_dbc::unexpected(beginResult.error());
                    }
                }
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SAC1A2B3C4D5",
                                                   std::string("setAutoCommit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SAC1A2B3C4D6",
                                                   "setAutoCommit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        try
        {
            return m_autoCommit.load(std::memory_order_acquire);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GAC1A2B3C4D5",
                                                   std::string("getAutoCommit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GAC1A2B3C4D6",
                                                   "getAutoCommit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("FD82C45A3E09", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            if (m_transactionActive.load(std::memory_order_acquire))
            {
                return false;
            }

            if (m_autoCommit.load(std::memory_order_acquire))
            {
                m_autoCommit.store(false, std::memory_order_release);
            }

            auto updateResult = executeUpdate(std::nothrow, "BEGIN TRANSACTION");
            if (!updateResult)
            {
                return cpp_dbc::unexpected(updateResult.error());
            }

            m_transactionActive.store(true, std::memory_order_release);
            return true;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("BT1A1B2C3D4E",
                                                   std::string("beginTransaction failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("BT1A1B2C3D4F",
                                                   "beginTransaction failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        try
        {
            return m_transactionActive.load(std::memory_order_acquire);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("TA1A1B2C3D4E",
                                                   std::string("transactionActive failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("TA1A1B2C3D4F",
                                                   "transactionActive failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::commit(std::nothrow_t) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("R6Z7A8B9C0D1", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            auto updateResult = executeUpdate(std::nothrow, "COMMIT");
            if (!updateResult)
            {
                return cpp_dbc::unexpected(updateResult.error());
            }

            m_transactionActive.store(false, std::memory_order_release);
            m_autoCommit.store(true, std::memory_order_release);
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("CO1A1B2C3D4E",
                                                   std::string("commit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("CO1A1B2C3D4F",
                                                   "commit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::rollback(std::nothrow_t) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("R7Z8A9B0C1D2", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            auto updateResult = executeUpdate(std::nothrow, "ROLLBACK");
            if (!updateResult)
            {
                return cpp_dbc::unexpected(updateResult.error());
            }

            m_transactionActive.store(false, std::memory_order_release);
            m_autoCommit.store(true, std::memory_order_release);
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RB1A1B2C3D4E",
                                                   std::string("rollback failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RB1A1B2C3D4F",
                                                   "rollback failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed || !m_db)
            {
                return cpp_dbc::unexpected(DBException("R8Z9A0B1C2D3", "Connection is closed",
                                                       system_utils::captureCallStack()));
            }

            // SQLite only supports SERIALIZABLE isolation level
            if (level != TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
            {
                return cpp_dbc::unexpected(DBException("R9Z0A1B2C3D4", "SQLite only supports SERIALIZABLE isolation level",
                                                       system_utils::captureCallStack()));
            }

            this->m_isolationLevel = level;
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("STI1A2B3C4D5",
                                                   std::string("setTransactionIsolation failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("STI1A2B3C4D6",
                                                   "setTransactionIsolation failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> SQLiteDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        try
        {
            return m_isolationLevel;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GTI1A2B3C4D5",
                                                   std::string("getTransactionIsolation failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GTI1A2B3C4D6",
                                                   "getTransactionIsolation failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::close(std::nothrow_t) noexcept
    {
        try
        {
            if (!m_closed && m_db)
            {
                // Close all result sets FIRST (before statements)
                // closeAllResultSets() acquires m_globalFileMutex internally
                [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);

                // Close all prepared statements properly
                // closeAllStatements() includes safety net for leaked statements
                [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

                // Call sqlite3_release_memory to free up caches and unused memory
                [[maybe_unused]]
                int releasedMemory = sqlite3_release_memory(1000000);
                SQLITE_DEBUG("Released %d bytes of SQLite memory", releasedMemory);

                // Smart pointer will automatically call sqlite3_close_v2 via SQLiteDbDeleter
                m_db.reset();
                m_closed = true;

                // Sleep for 10ms to avoid problems with concurrency and memory stability
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            // Ensure cleanup even on error
            m_db.reset();
            m_closed = true;
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            SQLITE_DEBUG("Exception during SQLite close: %s", ex.what());
            m_db.reset();
            m_closed = true;
            return cpp_dbc::unexpected(DBException("GRFPVO09UYNU",
                                                   std::string("close failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            m_db.reset();
            m_closed = true;
            return cpp_dbc::unexpected(DBException("FHR9J06XNVWS",
                                                   "close failed with unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed;
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        try
        {
            // Reset the connection state for the next borrower
            // reset() closes result sets, statements, rolls back and resets autocommit
            auto resetResult = reset(std::nothrow);
            if (!resetResult)
            {
                SQLITE_DEBUG("returnToPool(nothrow): reset failed: %s",
                             resetResult.error().what_s().data());
                // Continue - try to at least restore autocommit
            }

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("EYX8UAKL1E9B",
                                                   std::string("returnToPool failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("2XFKGOI6JLT8",
                                                   "returnToPool failed with unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBConnection::getURL(std::nothrow_t) const noexcept
    {
        return m_url;
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        // Delegate to reset() which closes result sets, statements, rolls back, and resets autocommit
        return reset(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // No-op for SQLite: no MVCC snapshot refresh needed
        return {};
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
