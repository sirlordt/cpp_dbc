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

    // Nothrow API
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

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::next(std::nothrow_t) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed)
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
                // Error occurred
                return cpp_dbc::unexpected(DBException("SL1A2B3C4D5E", "Error stepping through SQLite result set: " + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))),
                                                       system_utils::captureCallStack()));
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SL2B3C4D5E6F",
                                                   std::string("next failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SL3C4D5E6F7G",
                                                   "next failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition == 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("IBFA1B2C3D4E",
                                                   std::string("isBeforeFirst failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("IBFA1B2C3D4F",
                                                   "isBeforeFirst failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition > 0 && !m_hasData;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("IALA1B2C3D4E",
                                                   std::string("isAfterLast failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("IALA1B2C3D4F",
                                                   "isAfterLast failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> SQLiteDBResultSet::getRow(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GR1A1B2C3D4E",
                                                   std::string("getRow failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GR1A1B2C3D4F",
                                                   "getRow failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GI1A1B2C3D4E",
                                                   std::string("getInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GI1A1B2C3D4F",
                                                   "getInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("SLIR8S9T0U1V", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GIN1A2B3C4D5",
                                                   std::string("getInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GIN1A2B3C4D6",
                                                   "getInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
