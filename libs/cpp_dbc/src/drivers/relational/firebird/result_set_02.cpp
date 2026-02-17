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

 @file result_set_02.cpp
 @brief Firebird database driver implementation - Part 2: FirebirdDBResultSet throwing methods

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

    bool FirebirdDBResultSet::next()
    {
        auto result = next(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::isBeforeFirst()
    {
        auto result = isBeforeFirst(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::isAfterLast()
    {
        auto result = isAfterLast(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t FirebirdDBResultSet::getRow()
    {
        auto result = getRow(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    int FirebirdDBResultSet::getInt(size_t columnIndex)
    {
        auto result = getInt(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    int FirebirdDBResultSet::getInt(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("C9D5E1F7A4B0", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getInt(it->second);
    }

    int64_t FirebirdDBResultSet::getLong(size_t columnIndex)
    {
        auto result = getLong(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t FirebirdDBResultSet::getLong(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("D0E6F2A8B5C1", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getLong(it->second);
    }

    double FirebirdDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = getDouble(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    double FirebirdDBResultSet::getDouble(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("E1F7A3B9C6D2", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getDouble(it->second);
    }

    std::string FirebirdDBResultSet::getString(size_t columnIndex)
    {
        auto result = getString(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getString(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("F2A8B4C0D7E3", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getString(it->second);
    }

    bool FirebirdDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = getBoolean(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::getBoolean(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("A3B9C5D1E8F4", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getBoolean(it->second);
    }

    bool FirebirdDBResultSet::isNull(size_t columnIndex)
    {
        auto result = isNull(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::isNull(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("C5D1E7F3A0B6", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return isNull(it->second);
    }

    std::string FirebirdDBResultSet::getDate(size_t columnIndex)
    {
        auto result = getDate(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getDate(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("KNA6XOZ8SB0Z", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getDate(it->second);
    }

    std::string FirebirdDBResultSet::getTimestamp(size_t columnIndex)
    {
        auto result = getTimestamp(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getTimestamp(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("EVTK1VFQCYOU", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getTimestamp(it->second);
    }

    std::string FirebirdDBResultSet::getTime(size_t columnIndex)
    {
        auto result = getTime(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getTime(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("1I5VNYYG4KTH", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getTime(it->second);
    }

    std::vector<std::string> FirebirdDBResultSet::getColumnNames()
    {
        auto result = getColumnNames(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    size_t FirebirdDBResultSet::getColumnCount()
    {
        auto result = getColumnCount(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

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

    void FirebirdDBResultSet::close()
    {
        auto result = close(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    bool FirebirdDBResultSet::isEmpty()
    {

        FIREBIRD_LOCK_OR_THROW("GTVGJY1OO1PS", "Connection lost");

        return !m_hasData && m_rowPosition == 0;
    }

    std::shared_ptr<Blob> FirebirdDBResultSet::getBlob(size_t columnIndex)
    {

        FIREBIRD_LOCK_OR_THROW("BR4IRYH99OLH", "Connection lost");

        if (columnIndex >= m_fieldCount)
        {
            throw DBException("D6E2F8A4B1C7", "Column index out of range: " + std::to_string(columnIndex),
                              system_utils::captureCallStack());
        }

        if (m_nullIndicators[columnIndex] < 0)
        {
            return nullptr;
        }

        XSQLVAR *var = &m_sqlda->sqlvar[columnIndex];
        if ((var->sqltype & ~1) != SQL_BLOB)
        {
            throw DBException("E7F3A9B5C2D8", "Column is not a BLOB type", system_utils::captureCallStack());
        }

        auto conn = m_connection.lock();
        if (!conn)
        {
            throw DBException("F8A4B0C6D3E9", "Connection has been closed", system_utils::captureCallStack());
        }

        ISC_QUAD *blobId = reinterpret_cast<ISC_QUAD *>(var->sqldata);
        // Use the connection-based constructor for memory safety
        return std::make_shared<FirebirdBlob>(conn, *blobId);
    }

    std::shared_ptr<Blob> FirebirdDBResultSet::getBlob(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("A9B5C1D7E4F0", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getBlob(it->second);
    }

    std::shared_ptr<InputStream> FirebirdDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto blob = getBlob(columnIndex);
        if (!blob)
            return nullptr;
        return blob->getBinaryStream();
    }

    std::shared_ptr<InputStream> FirebirdDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("B0C6D2E8F5A1", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getBinaryStream(it->second);
    }

    std::vector<uint8_t> FirebirdDBResultSet::getBytes(size_t columnIndex)
    {
        auto blob = getBlob(columnIndex);
        if (!blob)
            return std::vector<uint8_t>();
        return blob->getBytes(0, blob->length());
    }

    std::vector<uint8_t> FirebirdDBResultSet::getBytes(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("C1D7E3F9A6B2", "Column not found: " + columnName, system_utils::captureCallStack());
        }
        return getBytes(it->second);
    }

    // ============================================================================
    // FirebirdDBResultSet - NOTHROW METHODS (part 1)
    // ============================================================================

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

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
