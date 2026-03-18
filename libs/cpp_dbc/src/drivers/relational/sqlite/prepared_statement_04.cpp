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

 @file prepared_statement_04.cpp
 @brief SQLite database driver implementation - SQLiteDBPreparedStatement nothrow methods (part 3 - execute and close)

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

    // Execute methods for SQLiteDBPreparedStatement - Nothrow API
    // No try/catch: all inner calls are nothrow (scoped_lock, C API calls, nothrow create).
    // Only death-sentence exceptions possible.

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> SQLiteDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("SL6F7G8H9I0J", "Statement is closed");

        auto dbPtrResult = getSQLiteConnection(std::nothrow);
        if (!dbPtrResult.has_value())
        {
            return cpp_dbc::unexpected(dbPtrResult.error());
        }
        sqlite3 *dbPtr = dbPtrResult.value();

        int resetResult = sqlite3_reset(m_stmt.get());
        if (resetResult != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("SL7G8H9I0J1K", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                   system_utils::captureCallStack()));
        }

        auto rsResult = SQLiteDBResultSet::create(std::nothrow, m_stmt.get(), false, m_connection.lock(), shared_from_this());
        if (!rsResult.has_value())
        {
            return cpp_dbc::unexpected(rsResult.error());
        }
        return std::shared_ptr<RelationalDBResultSet>(rsResult.value());
    }

    cpp_dbc::expected<uint64_t, DBException> SQLiteDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("SL8H9I0J1K2L", "Statement is closed");

        auto dbPtrResult = getSQLiteConnection(std::nothrow);
        if (!dbPtrResult.has_value())
        {
            return cpp_dbc::unexpected(dbPtrResult.error());
        }
        sqlite3 *dbPtr = dbPtrResult.value();

        int resetResult = sqlite3_reset(m_stmt.get());
        if (resetResult != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("SL9I0J1K2L3M", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                   system_utils::captureCallStack()));
        }

        int result = sqlite3_step(m_stmt.get());
        if (result != SQLITE_DONE)
        {
            return cpp_dbc::unexpected(DBException("SLAJ0K1L2M3N", "Failed to execute update: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(result) + ")",
                                                   system_utils::captureCallStack()));
        }

        uint64_t changes = sqlite3_changes(dbPtr);

        resetResult = sqlite3_reset(m_stmt.get());
        if (resetResult != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("SLBK1L2M3N4O", "Failed to reset statement after execution: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                   system_utils::captureCallStack()));
        }

        return changes;
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBPreparedStatement::execute(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("SLCL2M3N4O5P", "Statement is closed");

        auto dbPtrResult = getSQLiteConnection(std::nothrow);
        if (!dbPtrResult.has_value())
        {
            return cpp_dbc::unexpected(dbPtrResult.error());
        }
        sqlite3 *dbPtr = dbPtrResult.value();

        int resetResult = sqlite3_reset(m_stmt.get());
        if (resetResult != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("YQM80QQADT4X", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                   system_utils::captureCallStack()));
        }

        int result = sqlite3_step(m_stmt.get());

        if (result == SQLITE_ROW)
        {
            sqlite3_reset(m_stmt.get());
            return true;
        }
        else if (result == SQLITE_DONE)
        {
            return false;
        }
        else
        {
            return cpp_dbc::unexpected(DBException("SLDM3N4O5P6Q", "Failed to execute statement: " + std::string(sqlite3_errmsg(dbPtr)),
                                                   system_utils::captureCallStack()));
        }
    }

    // No try/catch: all inner calls are nothrow (SQLiteConnectionLock, C API calls, atomic store,
    // closeAllResultSets(nothrow)). Only death-sentence exceptions possible.
    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::close(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // Close all child result sets FIRST (before closing statement)
        [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);

        if (m_stmt)
        {
            auto dbPtr = m_conn.lock();
            if (dbPtr)
            {
                int resetResult = sqlite3_reset(m_stmt.get());
                if (resetResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("7K8L9M0N1O2P: Error resetting SQLite statement: %s",
                                 sqlite3_errstr(resetResult));
                }

                int clearResult = sqlite3_clear_bindings(m_stmt.get());
                if (clearResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("3Q4R5S6T7U8V: Error clearing SQLite statement bindings: %s",
                                 sqlite3_errstr(clearResult));
                }

                m_stmt.reset();
            }
            else
            {
                m_stmt.release();
                SQLITE_DEBUG("5C6D7E8F9G0H: Connection closed, releasing statement without finalize");
            }
        }
        m_closed.store(true, std::memory_order_seq_cst);
        return {};
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
