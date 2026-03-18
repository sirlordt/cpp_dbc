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

 @file result_set_02.cpp
 @brief SQLite database driver implementation - SQLiteDBResultSet nothrow methods (part 1)

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

    // No try/catch: all inner calls are nothrow (SQLiteConnectionLock, C API calls, atomic store,
    // isClosed(nothrow), unregisterResultSet(nothrow)). Only death-sentence exceptions possible.
    cpp_dbc::expected<void, DBException> SQLiteDBResultSet::close(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        if (m_ownStatement && m_stmt)
        {
            // Check if the connection is still active using nothrow API
            auto conn = m_connection.lock();
            auto closedResult = conn ? conn->isClosed(std::nothrow) : cpp_dbc::expected<bool, DBException>(true);
            bool connectionValid = closedResult.has_value() && !closedResult.value();

            if (connectionValid)
            {
                // Reset the statement first to ensure all data is cleared
                int resetResult = sqlite3_reset(m_stmt);
                if (resetResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("7A8B9C0D1E2F: Error resetting SQLite statement: %s",
                                 sqlite3_errstr(resetResult));
                }

                // Finalize the statement - only if connection is still valid
                // If connection is closed, it already finalized all statements via sqlite3_next_stmt()
                int finalizeResult = sqlite3_finalize(m_stmt);
                if (finalizeResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("8H9I0J1K2L3M: Error finalizing SQLite statement: %s",
                                 sqlite3_errstr(finalizeResult));
                }
            }
            else
            {
                SQLITE_DEBUG("5M6N7O8P9Q0R: SQLiteResultSet::close - Connection is closed or invalid, skipping statement finalization");
                // Don't finalize - connection already did it
            }
        }

        // Clear the pointer
        m_stmt = nullptr;

        // Mark as closed before clearing data
        m_closed.store(true, std::memory_order_seq_cst);

        // Clear any cached data
        m_columnNames.clear();
        m_columnMap.clear();

        // Unregister from Connection
        if (auto conn = m_connection.lock())
        {
            [[maybe_unused]] auto unregResult = conn->unregisterResultSet(std::nothrow, weak_from_this());
        }

        // Unregister from PreparedStatement
        if (auto prepStmt = m_preparedStatement.lock())
        {
            [[maybe_unused]] auto unregResult = prepStmt->unregisterResultSet(std::nothrow, weak_from_this());
        }

        // Clear the references
        m_connection.reset();
        m_preparedStatement.reset();

        return {};
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isEmpty(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("SL0E1F2G3H4I", "Result set is closed");
        return m_rowPosition == 0 && !m_hasData;
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::next(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("SL1A2B3C4D5E", "Result set is closed");

        sqlite3_stmt *stmt = getStmt(std::nothrow);
        if (!stmt)
        {
            return false;
        }

        int result = sqlite3_step(stmt);

        if (result == SQLITE_ROW)
        {
            m_rowPosition++;
            m_hasData = true;
            return true;
        }
        else if (result == SQLITE_DONE)
        {
            m_rowPosition++;
            m_hasData = false;
            return false;
        }
        else
        {
            return cpp_dbc::unexpected(DBException("SL1A2B3C4D5E",
                "Error stepping through SQLite result set: " + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))),
                system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("2DVXWAOV5X23", "Cannot check position");
        return m_rowPosition == 0;
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("RCL9SMIMODWQ", "Cannot check position");
        return m_rowPosition > 0 && !m_hasData;
    }

    cpp_dbc::expected<uint64_t, DBException> SQLiteDBResultSet::getRow(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("P1LQEOHYKORE", "Cannot get row");
        return m_rowPosition;
    }

    cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("0A7LU8DS9WF3", "Result set is closed");

        sqlite3_stmt *stmt = getStmt(std::nothrow);
        if (!stmt || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("0A7LU8DS9WF3", "Invalid column index or row position",
                                                   system_utils::captureCallStack()));
        }

        // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
        int idx = static_cast<int>(columnIndex - 1);

        if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
        {
            return 0; // Return 0 for NULL values (similar to JDBC)
        }

        return sqlite3_column_int(stmt, idx);
    }

    cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("SLIR8S9T0U1V", "Column not found: " + columnName,
                                                   system_utils::captureCallStack()));
        }

        return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
