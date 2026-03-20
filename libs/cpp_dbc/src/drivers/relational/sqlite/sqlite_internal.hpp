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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file sqlite_internal.hpp
 * @brief SQLite driver internal utilities - not part of public API
 */

#ifndef CPP_DBC_SQLITE_INTERNAL_HPP
#define CPP_DBC_SQLITE_INTERNAL_HPP

#if USE_SQLITE

#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstring>
#include "cpp_dbc/common/system_utils.hpp"

// SQLite uses m_globalFileMutex (file-level mutex from FileMutexRegistry) instead of
// m_connMutex. The lock is always acquired regardless of DB_DRIVER_THREAD_SAFE because
// the file mutex exists for sanitizer compatibility and cross-connection synchronization.

// For nothrow SQLiteDBConnection methods — locks file mutex and returns unexpected if closed
// Guard: if m_globalFileMutex was never initialized (constructor failed before mutex init),
// there's nothing to do — m_conn is also null and the connection is in a failed state.
#define SQLITE_CONNECTION_LOCK_OR_RETURN(mark, msg)                                         \
    if (!m_globalFileMutex)                                                                 \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection not initialized)",   \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }                                                                                       \
    std::scoped_lock lock(*m_globalFileMutex);                                              \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)                                  \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

// For throwing SQLiteDBConnection methods — locks file mutex and throws if closed
// Guard: if m_globalFileMutex was never initialized, throw (connection not initialized).
#define SQLITE_CONNECTION_LOCK_OR_THROW(mark, msg)                                           \
    if (!m_globalFileMutex)                                                                  \
    {                                                                                        \
        throw DBException(mark, msg " (connection not initialized)",                         \
                          cpp_dbc::system_utils::captureCallStack());                        \
    }                                                                                        \
    std::scoped_lock lock(*m_globalFileMutex);                                               \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)                                 \
    {                                                                                        \
        throw DBException(mark, msg " (connection closed)",                                  \
                          cpp_dbc::system_utils::captureCallStack());                        \
    }

// For close() — locks file mutex and returns success if already closed (idempotent)
// Guard: if m_globalFileMutex was never initialized, nothing to close.
#define SQLITE_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                                \
    if (!m_globalFileMutex)                                                                 \
    {                                                                                       \
        return {}; /* Mutex never initialized = nothing to close */                         \
    }                                                                                       \
    std::scoped_lock lock(*m_globalFileMutex);                                              \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)                                  \
    {                                                                                       \
        return {}; /* Already closed = success (idempotent) */                              \
    }

// ============================================================================
// SQLiteConnectionLock - RAII Helper for PreparedStatement and ResultSet
// ============================================================================
// Automatically acquires the connection mutex through the weak_ptr<Connection>.
// If the connection is destroyed, marks the object as closed and fails gracefully.
// Usage:
//   SQLITE_STMT_LOCK_OR_RETURN(error_code, "error message");
//
// Unlike MySQL/PG/Firebird, this is NOT conditional on DB_DRIVER_THREAD_SAFE
// because SQLite's file-level mutex is always required (sanitizer compatibility
// and cursor-based iteration model).
// ============================================================================

namespace cpp_dbc::SQLite
{
    // Forward declaration
    class SQLiteDBConnection;

    /**
     * @brief RAII lock helper that obtains mutex through weak_ptr<SQLiteDBConnection>
     *
     * This class implements the requirement that PreparedStatement and ResultSet
     * access the global lock ONLY through their parent DBConnection weak_ptr.
     *
     * If the connection is destroyed (weak_ptr expired), it automatically marks
     * the object as closed instead of crashing.
     *
     * IMPORTANT: This class keeps the DBConnection alive via shared_ptr while the lock
     * is held. This is necessary because the mutex itself is a shared_ptr owned by
     * DBConnection, and we need to prevent the mutex from being destroyed while locked.
     */
    class SQLiteConnectionLock
    {
        std::shared_ptr<SQLiteDBConnection> m_conn; // Keep connection alive while lock is held
        std::unique_lock<std::recursive_mutex> m_lock;
        bool m_acquired{false};

    public:
        /**
         * @brief Acquire lock through connection weak_ptr with double-checked locking
         * @param obj The object (PreparedStatement or ResultSet) that has m_connection member
         * @param closed Atomic flag to check/mark object closed state
         *
         * Thread-safe flow:
         * 1. Check if object is already closed (fast path, no lock needed)
         * 2. Try to obtain connection from weak_ptr and KEEP IT ALIVE
         * 3. Acquire connection's mutex
         * 4. Check again if closed (in case another thread closed between step 1 and 3)
         *
         * The connection is kept alive via m_conn shared_ptr until this lock object
         * is destroyed, preventing the mutex from being destroyed while locked.
         */
        template <typename T>
        SQLiteConnectionLock(T *obj, std::atomic<bool> &closed)
        {
            // FIRST CHECK: Verify if object is already closed (fast path, no lock needed)
            if (closed.load(std::memory_order_seq_cst))
            {
                m_acquired = false;
                return;
            }

            // Try to obtain connection from weak_ptr and KEEP IT ALIVE
            m_conn = obj->m_connection.lock();
            if (!m_conn)
            {
                // Connection is destroyed — mark object as closed
                closed.store(true, std::memory_order_seq_cst);
                m_acquired = false;
                return;
            }

            // Acquire the connection's mutex (connection stays alive via m_conn)
            m_lock = std::unique_lock<std::recursive_mutex>(m_conn->getConnectionMutex(std::nothrow));

            // SECOND CHECK: Verify again after acquiring lock
            // (Another thread could have closed the object between first check and lock acquisition)
            if (closed.load(std::memory_order_seq_cst))
            {
                m_acquired = false;
                // Lock is released automatically by m_lock destructor
                // Connection will be released when m_conn goes out of scope
                return;
            }

            // THIRD CHECK: Verify parent connection is still open
            // The connection object may still exist (weak_ptr succeeded) but be explicitly
            // closed (m_closed == true or m_conn == null). Reject operations in this case.
            if (m_conn->m_closed.load(std::memory_order_seq_cst) || !m_conn->m_conn)
            {
                closed.store(true, std::memory_order_seq_cst);
                m_acquired = false;
                return;
            }

            m_acquired = true;
        }

        bool isAcquired() const noexcept
        {
            return m_acquired;
        }

        explicit operator bool() const noexcept
        {
            return m_acquired;
        }
    };

} // namespace cpp_dbc::SQLite

// Macro for nothrow PreparedStatement/ResultSet methods - returns unexpected if lock fails
#define SQLITE_STMT_LOCK_OR_RETURN(mark, msg)                                                    \
    cpp_dbc::SQLite::SQLiteConnectionLock sqlite_conn_lock_(this, m_closed);                 \
    if (!sqlite_conn_lock_)                                                                 \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

// Macro for throwing PreparedStatement/ResultSet methods - throws if lock fails
#define SQLITE_STMT_LOCK_OR_THROW(mark, msg)                                                             \
    cpp_dbc::SQLite::SQLiteConnectionLock sqlite_conn_lock_(this, m_closed);                         \
    if (!sqlite_conn_lock_)                                                                         \
    {                                                                                               \
        throw DBException(mark, msg " (connection closed)", cpp_dbc::system_utils::captureCallStack()); \
    }

// Macro for close() methods - returns success if already closed (idempotent close)
#define SQLITE_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                                           \
    cpp_dbc::SQLite::SQLiteConnectionLock sqlite_conn_lock_(this, m_closed);                 \
    if (!sqlite_conn_lock_)                                                                 \
    {                                                                                       \
        return {}; /* Already closed or connection lost = success */                         \
    }

// Debug output is controlled by -DDEBUG_SQLITE=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_SQLITE) && DEBUG_SQLITE) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define SQLITE_DEBUG(format, ...)                                                     \
    do                                                                                \
    {                                                                                 \
        char debug_buffer[1024];                                                      \
        int sqlite_debug_n = std::snprintf(debug_buffer, sizeof(debug_buffer), format, ##__VA_ARGS__); \
        if (sqlite_debug_n >= static_cast<int>(sizeof(debug_buffer)))                \
        {                                                                             \
            static constexpr const char sqlite_trunc[] = "...[TRUNCATED]";              \
            std::memcpy(debug_buffer + sizeof(debug_buffer) - sizeof(sqlite_trunc),      \
                        sqlite_trunc, sizeof(sqlite_trunc));                                  \
        }                                                                             \
        cpp_dbc::system_utils::logWithTimesMillis("SQLite", debug_buffer);            \
    } while (0)
#else
#define SQLITE_DEBUG(...) ((void)0)
#endif

#endif // USE_SQLITE

#endif // CPP_DBC_SQLITE_INTERNAL_HPP
