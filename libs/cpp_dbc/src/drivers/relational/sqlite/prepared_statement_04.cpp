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
#include "cpp_dbc/drivers/relational/sqlite_blob.hpp"
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
    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> SQLiteDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("SL6F7G8H9I0J", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(m_stmt.get());
            if (resetResult != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("SL7G8H9I0J1K", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                       system_utils::captureCallStack()));
            }

#if DB_DRIVER_THREAD_SAFE
            // Pass shared mutex to ResultSet - required because SQLite uses cursor-based iteration
            // where sqlite3_step() and sqlite3_column_*() access the connection handle on every call.
            // Unlike MySQL/PostgreSQL where results are fully loaded into client memory.
            auto resultSet = std::make_shared<SQLiteDBResultSet>(m_stmt.get(), false, nullptr, m_connMutex);
#else
            auto resultSet = std::make_shared<SQLiteDBResultSet>(m_stmt.get(), false, nullptr);
#endif
            return std::shared_ptr<RelationalDBResultSet>(resultSet);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("EQ1A1B2C3D4E",
                                                   std::string("executeQuery failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("EQ1A1B2C3D4F",
                                                   "executeQuery failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> SQLiteDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("SL8H9I0J1K2L", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(m_stmt.get());
            if (resetResult != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("SL9I0J1K2L3M", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                       system_utils::captureCallStack()));
            }

            // Execute the statement
            int result = sqlite3_step(m_stmt.get());
            if (result != SQLITE_DONE)
            {
                return cpp_dbc::unexpected(DBException("SLAJ0K1L2M3N", "Failed to execute update: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(result) + ")",
                                                       system_utils::captureCallStack()));
            }

            // Get the number of affected rows
            uint64_t changes = sqlite3_changes(dbPtr);

            // Reset the statement after execution to allow reuse
            resetResult = sqlite3_reset(m_stmt.get());
            if (resetResult != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("SLBK1L2M3N4O", "Failed to reset statement after execution: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                       system_utils::captureCallStack()));
            }

            return changes;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("EU1A1B2C3D4E",
                                                   std::string("executeUpdate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("EU1A1B2C3D4F",
                                                   "executeUpdate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBPreparedStatement::execute(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("SLCL2M3N4O5P", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Reset the statement to ensure it's ready for execution
            sqlite3_reset(m_stmt.get());

            // Execute the statement
            int result = sqlite3_step(m_stmt.get());

            if (result == SQLITE_ROW)
            {
                // There are rows to fetch, so this is a query
                sqlite3_reset(m_stmt.get());
                return true;
            }
            else if (result == SQLITE_DONE)
            {
                // No rows to fetch, so this is an update
                return false;
            }
            else
            {
                return cpp_dbc::unexpected(DBException("SLDM3N4O5P6Q", "Failed to execute statement: " + std::string(sqlite3_errmsg(dbPtr)),
                                                       system_utils::captureCallStack()));
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("EX1A1B2C3D4E",
                                                   std::string("execute failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("EX1A1B2C3D4F",
                                                   "execute failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::close(std::nothrow_t) noexcept
    {
        try
        {
            // CRITICAL: Must hold the shared connection mutex to prevent race conditions.
            // sqlite3_finalize() uses the sqlite3* connection handle, so concurrent access from
            // another thread (e.g., connection pool validation) causes undefined behavior.
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (!m_closed && m_stmt)
            {
                auto dbPtr = m_db.lock();
                if (dbPtr)
                {
                    int resetResult = sqlite3_reset(m_stmt.get());
                    if (resetResult != SQLITE_OK)
                    {
                        SQLITE_DEBUG("7K8L9M0N1O2P: Error resetting SQLite statement: "
                                     << sqlite3_errstr(resetResult));
                    }

                    int clearResult = sqlite3_clear_bindings(m_stmt.get());
                    if (clearResult != SQLITE_OK)
                    {
                        SQLITE_DEBUG("3Q4R5S6T7U8V: Error clearing SQLite statement bindings: "
                                     << sqlite3_errstr(clearResult));
                    }

                    m_stmt.reset();
                }
                else
                {
                    m_stmt.release();
                    SQLITE_DEBUG("5C6D7E8F9G0H: Connection closed, releasing statement without finalize");
                }
            }
            m_closed = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("CL1A1B2C3D4E",
                                                   std::string("close failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("CL1A1B2C3D4F",
                                                   "close failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
