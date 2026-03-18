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

 @file result_set_04.cpp
 @brief Firebird database driver implementation - FirebirdDBResultSet nothrow methods: date/time pairs, metadata, and lifecycle

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

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getDate(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("7OAQLW6S1NV5", "Connection lost");
        return getColumnValue(std::nothrow, columnIndex);
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getDate(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("SQW4887JE3NZ", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getDate(std::nothrow, it->second);
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("4Q9KI4JP2XTB", "Connection lost");
        return getColumnValue(std::nothrow, columnIndex);
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getTimestamp(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("59TXOU10MU3H", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getTimestamp(std::nothrow, it->second);
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getTime(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("ZRWB1E4M2L52", "Connection lost");
        return getColumnValue(std::nothrow, columnIndex);
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getTime(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("H0N3VUR5RPZH", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getTime(std::nothrow, it->second);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> FirebirdDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("FXE1E4KBWNL8", "Connection lost");
        return m_columnNames;
    }

    cpp_dbc::expected<size_t, DBException> FirebirdDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("HJ4C76QCZV7E", "Connection lost");
        return m_fieldCount;
    }

    cpp_dbc::expected<void, DBException> FirebirdDBResultSet::close(std::nothrow_t) noexcept
    {
        // Use special macro for close() - returns success if already closed (idempotent)
        FIREBIRD_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // Unregister from connection if connection is still alive AND not in reset()
        // During reset(), closeAllResultSets() already holds the lock and clears the list
        auto conn = m_connection.lock();
        if (conn && !conn->isResetting())
        {
            [[maybe_unused]] auto unregResult = conn->unregisterResultSet(std::nothrow, weak_from_this());
        }

        // Mark as closed FIRST to prevent double-close attempts
        m_closed.store(true, std::memory_order_seq_cst);

        if (m_ownStatement)
        {
            // Only attempt to free if we own the statement
            if (m_stmt && m_stmt.get())
            {
                isc_stmt_handle *stmtPtr = m_stmt.get();

                if (stmtPtr && *stmtPtr != 0)
                {
                    // Free the statement
                    ISC_STATUS_ARRAY status = {0};
                    isc_dsql_free_statement(status, stmtPtr, DSQL_drop);

                    // CRITICAL: Add delay after freeing
                    // isc_dsql_free_statement is asynchronous in Firebird
                    // Without this delay, the transaction may end before Firebird completes
                    // the statement freeing internally, causing crashes
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));

                    FIREBIRD_DEBUG("ResultSet::close - Statement freed with 25ms delay");
                }
            }
        }

        // Reset our member variables
        m_sqlda.reset();
        m_stmt.reset();

        return {};
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isEmpty(std::nothrow_t) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("VEMY08W2KUSH", "Connection lost");
        return !m_hasData && m_rowPosition == 0;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
