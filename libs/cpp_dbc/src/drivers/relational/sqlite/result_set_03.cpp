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

 @file result_set_03.cpp
 @brief SQLite database driver implementation - SQLiteDBResultSet nothrow methods (part 2 - blob/binary)

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

    cpp_dbc::expected<double, DBException> SQLiteDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("SL5E6F7G8H9I", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getDouble(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GDN1A2B3C4D5",
                                                   std::string("getDouble failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GDN1A2B3C4D6",
                                                   "getDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("93A82C42FA7B", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getString(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GSN1A2B3C4D5",
                                                   std::string("getString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GSN1A2B3C4D6",
                                                   "getString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto intResult = getInt(std::nothrow, columnName);
            if (!intResult)
            {
                return cpp_dbc::unexpected(intResult.error());
            }
            return *intResult != 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBN1A2B3C4D5",
                                                   std::string("getBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBN1A2B3C4D6",
                                                   "getBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("8BAE4B58A947", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return isNull(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("INN1A2B3C4D5",
                                                   std::string("isNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("INN1A2B3C4D6",
                                                   "isNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getDate(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("PBQCIWA2TJZ3", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getDate(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D3YH1LHQ1CBY",
                                                   std::string("getDate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("HBPSL6UPB6AH",
                                                   "getDate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTimestamp(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("R09JSU56AXFJ", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getTimestamp(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("22EKF4Q293YB",
                                                   std::string("getTimestamp failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("WC33FJ13ZU13",
                                                   "getTimestamp failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTime(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("F2901E52MFWC", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getTime(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("UG5NGYTUB1LR",
                                                   std::string("getTime failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("AILUOP4YRQAW",
                                                   "getTime failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> SQLiteDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        try
        {
            return m_columnNames;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GCN1A2B3C4D5",
                                                   std::string("getColumnNames failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GCN1A2B3C4D6",
                                                   "getColumnNames failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<size_t, DBException> SQLiteDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        try
        {
            return m_fieldCount;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GCC1A2B3C4D5",
                                                   std::string("getColumnCount failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GCC1A2B3C4D6",
                                                   "getColumnCount failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    // BLOB support methods for SQLiteDBResultSet
    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("B1C2D3E4F5G6", "Invalid column index or row position for getBlob",
                                                       system_utils::captureCallStack()));
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return std::shared_ptr<Blob>(std::make_shared<SQLite::SQLiteBlob>(std::shared_ptr<sqlite3>()));
            }

            // Check if the column is a BLOB type
            if (sqlite3_column_type(stmt, idx) != SQLITE_BLOB)
            {
                return cpp_dbc::unexpected(DBException("H7I8J9K0L1M2", "Column is not a BLOB type",
                                                       system_utils::captureCallStack()));
            }

            // Get the binary data
            const void *blobData = sqlite3_column_blob(stmt, idx);
            int blobSize = sqlite3_column_bytes(stmt, idx);

            // Create a vector with the data
            std::vector<uint8_t> data;
            if (blobData && blobSize > 0)
            {
                data.resize(blobSize);
                std::memcpy(data.data(), blobData, blobSize);
            }

            // Get a shared_ptr to the connection
            auto conn = m_connection.lock();
            if (!conn)
            {
                return cpp_dbc::unexpected(DBException("N3O4P5Q6R7S8", "Connection is no longer valid",
                                                       system_utils::captureCallStack()));
            }

            // Create a new BLOB object with the data (pass shared_ptr for safe reference)
            return std::shared_ptr<Blob>(std::make_shared<SQLite::SQLiteBlob>(conn->m_db, data));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBLO1A2B3C4D",
                                                   std::string("getBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBLO1A2B3C4E",
                                                   "getBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("T9U0V1W2X3Y4", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBLN1A2B3C4D",
                                                   std::string("getBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBLN1A2B3C4E",
                                                   "getBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("CEE30385E0BB", "Invalid column index or row position for getBinaryStream",
                                                       system_utils::captureCallStack()));
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                // Return an empty stream
                return std::shared_ptr<InputStream>(std::make_shared<SQLite::SQLiteInputStream>(nullptr, 0));
            }

            // Get the binary data
            const void *blobData = sqlite3_column_blob(stmt, idx);
            int blobSize = sqlite3_column_bytes(stmt, idx);

            // Create a new input stream with the data
            return std::shared_ptr<InputStream>(std::make_shared<SQLite::SQLiteInputStream>(blobData, blobSize));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBIS1A2B3C4D",
                                                   std::string("getBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBIS1A2B3C4E",
                                                   "getBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("F1G2H3I4J5K6", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBIN1A2B3C4D",
                                                   std::string("getBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBIN1A2B3C4E",
                                                   "getBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("L7M8N9O0P1Q2", "Invalid column index or row position for getBytes",
                                                       system_utils::captureCallStack()));
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return std::vector<uint8_t>{};
            }

            // Get the binary data
            const void *blobData = sqlite3_column_blob(stmt, idx);
            int blobSize = sqlite3_column_bytes(stmt, idx);

            // Create a vector with the data
            std::vector<uint8_t> data;
            if (blobData && blobSize > 0)
            {
                data.resize(blobSize);
                std::memcpy(data.data(), blobData, blobSize);
            }

            return data;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBYT1A2B3C4D",
                                                   std::string("getBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBYT1A2B3C4E",
                                                   "getBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("WDX9BB6U91FM", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getBytes(std::nothrow, it->second + 1); // +1 because getBytes(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBYN1A2B3C4D",
                                                   std::string("getBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBYN1A2B3C4E",
                                                   "getBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBResultSet::close(std::nothrow_t) noexcept
    {
        try
        {
            // CRITICAL: Must use global file-level mutex because sqlite3_reset() and
            // sqlite3_finalize() access the sqlite3* connection handle internally.
            // See class documentation for why SQLite needs global file-level synchronization.
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            if (m_closed)
            {
                return {};
            }

            if (m_ownStatement && m_stmt)
            {
                try
                {
                    // Verificar si la conexión todavía está activa
                    auto conn = m_connection.lock();
                    bool connectionValid = conn && !conn->isClosed();

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
                catch (const std::exception &e)
                {
                    SQLITE_DEBUG("9S0T1U2V3W4X: Exception during SQLite statement close: %s", e.what());
                }
                catch (...)
                {
                    SQLITE_DEBUG("5Y6Z7A8B9C0D: Unknown exception during SQLite statement close");
                }
            }

            // Clear the pointer
            m_stmt = nullptr;

            // Marcar como cerrado antes de limpiar los datos
            m_closed = true;

            // Clear any cached data
            m_columnNames.clear();
            m_columnMap.clear();

            // Unregister from Connection
            if (auto conn = m_connection.lock())
            {
                conn->unregisterResultSet(weak_from_this());
            }

            // Unregister from PreparedStatement
            if (auto prepStmt = m_preparedStatement.lock())
            {
                prepStmt->unregisterResultSet(weak_from_this());
            }

            // Clear the references
            m_connection.reset();
            m_preparedStatement.reset();

            // Sleep briefly to ensure resources are released
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("R7YA8VMHG4NC",
                                                   std::string("close failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("8C00UCX4083E",
                                                   "close failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isEmpty(std::nothrow_t) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);
            return m_rowPosition == 0 && !m_hasData;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F44RQTKWZ78Y",
                                                   std::string("isEmpty failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("WKDJ4L6FS7XF",
                                                   "isEmpty failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
