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

 @file prepared_statement_01.cpp
 @brief SQLite database driver implementation - SQLiteDBPreparedStatement (constructor, destructor, throwing methods)

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

    // SQLiteDBPreparedStatement implementation
    // Private methods
    void SQLiteDBPreparedStatement::notifyConnClosing(std::nothrow_t) noexcept
    {
        // Connection is closing, release the statement pointer without calling sqlite3_finalize
        // since the connection is already being destroyed and will clean up all statements
        m_stmt.release(); // Release without calling deleter
        m_closed.store(true, std::memory_order_seq_cst);
    }

    cpp_dbc::expected<sqlite3 *, DBException> SQLiteDBPreparedStatement::getSQLiteConnection(std::nothrow_t) const noexcept
    {
        auto conn = m_db.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("QQI768O7OI0F", "SQLite connection has been closed", system_utils::captureCallStack()));
        }
        return conn.get();
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::registerResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept
    {
        // LOCK CONSISTENCY FIX (2026-02-15): Changed from m_resultSetsMutex to
        // m_globalFileMutex to ensure ALL accesses to m_activeResultSets use the
        // same mutex. Previously, closeAllResultSets() used m_globalFileMutex while
        // registerResultSet()/unregisterResultSet() used m_resultSetsMutex, creating
        // a data race (same data protected by different mutexes in different contexts).
        //
        // Now ALL access to m_activeResultSets is consistently protected by
        // m_globalFileMutex, which is the file-level lock shared by all connections
        // to the same SQLite database file.
        SQLITE_STMT_LOCK_OR_RETURN("AWVN6T7OBL1H", "Cannot register result set");
        if (m_activeResultSets.size() > 20)  // Smaller threshold than Connection
        {
            std::erase_if(m_activeResultSets, [](const auto &w) { return w.expired(); });
        }
        m_activeResultSets.insert(rs);
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::unregisterResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept
    {
        // LOCK CONSISTENCY FIX (2026-02-15): Changed from m_resultSetsMutex to
        // m_globalFileMutex for consistency with closeAllResultSets() and
        // registerResultSet(). All modifications to m_activeResultSets must use
        // the same mutex to prevent data races.
        SQLITE_STMT_LOCK_OR_RETURN("IP2U0VDA3R85", "Cannot unregister result set");
        // Remove expired weak_ptrs and the specified one
        for (auto it = m_activeResultSets.begin(); it != m_activeResultSets.end();)
        {
            auto locked = it->lock();
            auto rsLocked = rs.lock();
            if (!locked || (rsLocked && locked.get() == rsLocked.get()))
            {
                it = m_activeResultSets.erase(it);
            }
            else
            {
                ++it;
            }
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::closeAllResultSets(std::nothrow_t) noexcept
    {
        // LOCK ORDER FIX (2026-02-15): Previously, this method acquired both
        // m_globalFileMutex and then m_resultSetsMutex (nested lock). This caused
        // Helgrind LockOrder violations because different code paths acquire locks
        // in different relative orders:
        //
        // Path 1 (from pool): pool mutex → m_globalFileMutex → m_resultSetsMutex
        // Path 2 (from destructor): m_globalFileMutex → m_resultSetsMutex
        //
        // This creates potential for deadlock if two threads simultaneously:
        //   Thread A: holds pool mutex, waits for m_globalFileMutex
        //   Thread B: holds m_globalFileMutex, waits for pool mutex (indirectly)
        //
        // SOLUTION: m_globalFileMutex is a file-level lock shared by all connections
        // to the same SQLite database file (via FileMutexRegistry). It protects ALL
        // access to the underlying sqlite3* connection. Since m_activeResultSets is
        // part of the prepared statement state and is only accessed when working with
        // the sqlite3* connection, it is already implicitly protected by
        // m_globalFileMutex. Therefore, the additional m_resultSetsMutex lock is
        // redundant and can be safely removed.
        //
        // SAFETY: Access to m_activeResultSets is now protected solely by
        // m_globalFileMutex, which is acquired by ALL methods that interact with
        // the SQLite prepared statement (execute, executeQuery, executeUpdate, etc.).
        // This provides sufficient synchronization without introducing lock ordering
        // issues.
        //
        // TRADE-OFF: This reduces lock granularity (broader lock scope) but eliminates
        // potential deadlocks. Since SQLite operations are typically fast and
        // m_globalFileMutex is already required for correctness with ThreadSanitizer,
        // this is an acceptable trade-off.
        //
        // Reference: Helgrind error logs in logs/test/2026-02-15-18-46-52/22_RUN02_fail.log
        SQLITE_STMT_LOCK_OR_RETURN("19GTGW7K56I0", "Cannot close result sets");

        // CRITICAL: Copy weak_ptrs to temporary vector to avoid iterator invalidation.
        // When we call rs->close(), it calls unregisterResultSet() which modifies
        // m_activeResultSets, invalidating iterators if we iterate directly.
        std::vector<std::weak_ptr<SQLiteDBResultSet>> resultSetsToClose;
        resultSetsToClose.reserve(m_activeResultSets.size());
        for (const auto &weak_rs : m_activeResultSets)
        {
            resultSetsToClose.push_back(weak_rs);
        }
        m_activeResultSets.clear();

        // Now close all result sets without holding the registry lock
        for (auto &weak_rs : resultSetsToClose)
        {
            auto rs = weak_rs.lock();
            if (rs)
            {
                // Use nothrow close — we are in a noexcept method
                [[maybe_unused]] auto closeResult = rs->close(std::nothrow);
            }
        }
        return {};
    }

    // ── Public nothrow constructor (PrivateCtorTag) ───────────────────────────────
    // sqlite3_prepare_v2 is a C API that never throws C++ exceptions.
    // No recoverable exceptions → no try/catch needed.
    SQLiteDBPreparedStatement::SQLiteDBPreparedStatement(
        SQLiteDBPreparedStatement::PrivateCtorTag,
        std::nothrow_t,
        std::weak_ptr<sqlite3> db,
        std::weak_ptr<SQLiteDBConnection> conn,
        std::shared_ptr<std::recursive_mutex> globalFileMutex,
        const std::string &sql) noexcept
        : m_db(std::move(db)), m_connection(std::move(conn)), m_sql(sql),
          m_globalFileMutex(std::move(globalFileMutex))
    {
        auto dbResult = getSQLiteConnection(std::nothrow);
        if (!dbResult.has_value())
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(std::move(dbResult.error()));
            return;
        }
        sqlite3 *dbPtr = dbResult.value();

        sqlite3_stmt *rawStmt = nullptr;
        int result = sqlite3_prepare_v2(dbPtr, m_sql.c_str(), -1, &rawStmt, nullptr);
        if (result != SQLITE_OK)
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("U0A1B2C3D4E5",
                "Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(dbPtr)),
                system_utils::captureCallStack());
            return;
        }

        // Transfer ownership to smart pointer
        m_stmt.reset(rawStmt);
        m_closed.store(false, std::memory_order_seq_cst);

        // Initialize BLOB-related vectors
        int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
        m_blobValues.resize(paramCount);
        m_blobObjects.resize(paramCount);
        m_streamObjects.resize(paramCount);
    }

    // ── Destructor ────────────────────────────────────────────────────────────────
    SQLiteDBPreparedStatement::~SQLiteDBPreparedStatement()
    {
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

    // ── Throwing API ──────────────────────────────────────────────────────────────
#ifdef __cpp_exceptions

    void SQLiteDBPreparedStatement::setInt(int parameterIndex, int value)
    {
        auto result = setInt(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setLong(int parameterIndex, int64_t value)
    {
        auto result = setLong(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setDouble(int parameterIndex, double value)
    {
        auto result = setDouble(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setString(int parameterIndex, const std::string &value)
    {
        auto result = setString(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setBoolean(int parameterIndex, bool value)
    {
        auto result = setBoolean(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setNull(int parameterIndex, [[maybe_unused]] Types type)
    {
        auto result = setNull(std::nothrow, parameterIndex, type);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
    {
        auto result = setDate(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
    {
        auto result = setTimestamp(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setTime(int parameterIndex, const std::string &value)
    {
        auto result = setTime(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
    {
        auto result = setBlob(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<RelationalDBResultSet> SQLiteDBPreparedStatement::executeQuery()
    {
        auto result = executeQuery(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t SQLiteDBPreparedStatement::executeUpdate()
    {
        auto result = executeUpdate(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBPreparedStatement::execute()
    {
        auto result = execute(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void SQLiteDBPreparedStatement::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
