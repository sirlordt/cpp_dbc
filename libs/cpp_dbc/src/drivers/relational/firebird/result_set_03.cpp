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

 @file result_set_03.cpp
 @brief Firebird database driver implementation - Part 6: FirebirdDBResultSet nothrow methods (part 1)

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
    // FirebirdDBResultSet - NOTHROW METHODS (part 1)
    // ============================================================================

    cpp_dbc::expected<void, cpp_dbc::DBException> FirebirdDBResultSet::close(std::nothrow_t) noexcept
    {
        try
        {
            // Use special macro for close() - returns success if already closed (idempotent)
            FIREBIRD_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

            // Unregister from connection if connection is still alive AND not in reset()
            // During reset(), closeAllActiveResultSets() already holds the lock and clears the list
            auto conn = m_connection.lock();
            if (conn && !conn->isResetting())
            {
                conn->unregisterResultSet(weak_from_this());
            }

            // Mark as closed FIRST to prevent double-close attempts
            m_closed.store(true, std::memory_order_release);

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

                        FIREBIRD_DEBUG("ResultSet::close - Statement freed with 5ms delay");
                    }
                }
            }

            // Reset our member variables
            m_sqlda.reset();
            m_stmt.reset();

            return {};
        }
        catch (const cpp_dbc::DBException &e)
        {
            FIREBIRD_DEBUG("ResultSet::close - DBException: %s", e.what_s().c_str());
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            FIREBIRD_DEBUG("ResultSet::close - Exception: %s", e.what());
            return cpp_dbc::unexpected(cpp_dbc::DBException("V1YTXD7VTIY7",
                                                            std::string("ResultSet close failed: ") + e.what(),
                                                            cpp_dbc::system_utils::captureCallStack()));
        }
        catch (...)
        {
            FIREBIRD_DEBUG("ResultSet::close - Unknown exception");
            return cpp_dbc::unexpected(cpp_dbc::DBException("7OKHO1WCVD4X",
                                                            "ResultSet close failed with unknown error",
                                                            cpp_dbc::system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isEmpty(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("VEMY08W2KUSH", "Connection lost");
            return !m_hasData && m_rowPosition == 0;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("SYWIX7DMFIQZ", std::string("Exception in isEmpty: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("2VAZEE71DBW6", "Unknown exception in isEmpty", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::next(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("ZALQADMF0DBS", "Connection lost");

            FIREBIRD_DEBUG("FirebirdResultSet::next - Starting");
            FIREBIRD_DEBUG("  m_closed: %s", (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt valid: %s", (m_stmt ? "yes" : "no"));

            if (m_closed.load(std::memory_order_acquire))
            {
                FIREBIRD_DEBUG("FirebirdResultSet::next - ResultSet is closed, returning false");
                return false;
            }

            if (!m_stmt)
            {
                FIREBIRD_DEBUG("FirebirdResultSet::next - m_stmt is null, returning false");
                return false;
            }

            if (!*m_stmt)
            {
                FIREBIRD_DEBUG("FirebirdResultSet::next - *m_stmt is 0 (invalid handle), returning false");
                return false;
            }

            FIREBIRD_DEBUG("  m_stmt handle value: %p", (void *)(uintptr_t)*m_stmt);
            FIREBIRD_DEBUG("  m_sqlda valid: %s", (m_sqlda ? "yes" : "no"));
            if (m_sqlda)
            {
                FIREBIRD_DEBUG("  m_sqlda->sqld: %d", (int)m_sqlda->sqld);
            }

            ISC_STATUS_ARRAY status;
            isc_stmt_handle *stmtPtr = m_stmt.get();
            FIREBIRD_DEBUG("  Calling isc_dsql_fetch with stmtPtr=%p, *stmtPtr=%p", (void *)stmtPtr, (void *)(uintptr_t)*stmtPtr);

            ISC_STATUS fetchStatus = isc_dsql_fetch(status, stmtPtr, SQL_DIALECT_V6, m_sqlda.get());
            FIREBIRD_DEBUG("  isc_dsql_fetch returned: %ld", (long)fetchStatus);

            if (fetchStatus == 0)
            {
                m_rowPosition++;
                m_hasData = true;
                FIREBIRD_DEBUG("FirebirdResultSet::next - Got row %llu", (unsigned long long)m_rowPosition);
                // Debug: print null indicators after fetch
                for (size_t i = 0; i < m_fieldCount; ++i)
                {
                    FIREBIRD_DEBUG("  After fetch - Column %zu: nullInd=%d, sqlind=%p, *sqlind=%d",
                                   i, (int)m_nullIndicators[i], (void *)m_sqlda->sqlvar[i].sqlind,
                                   (m_sqlda->sqlvar[i].sqlind ? (int)*m_sqlda->sqlvar[i].sqlind : -999));
                }
                return true;
            }
            else if (fetchStatus == 100)
            {
                m_hasData = false;
                FIREBIRD_DEBUG("FirebirdResultSet::next - No more rows (status 100)");
                return false;
            }
            else
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("FirebirdResultSet::next - Error: %s", errorMsg.c_str());
                return cpp_dbc::unexpected(DBException("B8C4D0E6F2A3", "Error fetching row: " + errorMsg,
                                                       system_utils::captureCallStack()));
            }
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("D835E3C2FEA7", std::string("Exception in next: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("FBEZ6A7B8C9D", "Unknown exception in next", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("GCBPQQTEZH46", "Connection lost");
            return m_rowPosition == 0 && !m_hasData;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E946F4D3AEB8", std::string("Exception in isBeforeFirst: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F057A5E4BFC9", "Unknown exception in isBeforeFirst", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("G1PFWP82F0TY", "Connection lost");
            return !m_hasData && m_rowPosition > 0;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A168B6F5CED0", std::string("Exception in isAfterLast: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B279C7A6DFE1", "Unknown exception in isAfterLast", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> FirebirdDBResultSet::getRow(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("49D6QLPRDFO5", "Connection lost");
            return m_rowPosition;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C38AD8B7E0F2", std::string("Exception in getRow: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D49BE9C8F1A3", "Unknown exception in getRow", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> FirebirdDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("KKJ6AU7N7GC0", "Connection lost");
            std::string value = getColumnValue(columnIndex);
            return value.empty() ? 0 : std::stoi(value);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E5ACFAD9B0C4", std::string("Exception in getInt: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F6BDE0EAC1D5", "Unknown exception in getInt", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> FirebirdDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("BKG4AMM82M79", "Connection lost");
            std::string value = getColumnValue(columnIndex);
            return value.empty() ? static_cast<int64_t>(0) : std::stoll(value);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("G7CEFBE1D2A6", std::string("Exception in getLong: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("H8DFA0C2E3B7", "Unknown exception in getLong", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> FirebirdDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("WW48OXWIIVBF", "Connection lost");
            FIREBIRD_DEBUG("getDouble(columnIndex=%zu)", columnIndex);
            std::string value = getColumnValue(columnIndex);
            FIREBIRD_DEBUG("  getColumnValue returned: '%s'", value.c_str());
            return value.empty() ? 0.0 : std::stod(value);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("I9E0B1D3F4C8", std::string("Exception in getDouble: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("J0F1C2E4A5D9", "Unknown exception in getDouble", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("FEKRO46JEMTL", "Connection lost");
            return getColumnValue(columnIndex);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("K1A2D3F5B6E0", std::string("Exception in getString: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("L2B3E4A6C7F1", "Unknown exception in getString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("C9VHRLKU1UYP", "Connection lost");
            std::string value = getColumnValue(columnIndex);
            if (value.empty())
                return false;

            if (value == "1" || value == "true" || value == "TRUE" || value == "T" || value == "t" || value == "Y" || value == "y")
                return true;
            return false;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("M3C4E5A7B8F2", std::string("Exception in getBoolean: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("N4D5F6B8C9A3", "Unknown exception in getBoolean", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("ICSJV6F47KZD", "Connection lost");
            if (columnIndex >= m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("B4C0D6E2F9A5", "Column index out of range: " + std::to_string(columnIndex),
                                                       system_utils::captureCallStack()));
            }
            return m_nullIndicators[columnIndex] < 0;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("F5C63D7F7AE5", std::string("Exception in isNull: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("09B6905763F3", "Unknown exception in isNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getDate(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("7OAQLW6S1NV5", "Connection lost");
            return getColumnValue(columnIndex);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("TVCWRV64XWN1", std::string("Exception in getDate: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("U2DONOCA2WXD", "Unknown exception in getDate", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("4Q9KI4JP2XTB", "Connection lost");
            return getColumnValue(columnIndex);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("NCMUNKXWLOPK", std::string("Exception in getTimestamp: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("Y9SOFG31I2BT", "Unknown exception in getTimestamp", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getTime(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("ZRWB1E4M2L52", "Connection lost");
            return getColumnValue(columnIndex);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("MS9E5DL4LFXS", std::string("Exception in getTime: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SHZXQU05NW4I", "Unknown exception in getTime", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
