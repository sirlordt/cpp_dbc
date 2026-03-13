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
 * @file postgresql_internal.hpp
 * @brief PostgreSQL driver internal utilities - not part of public API
 */

#ifndef CPP_DBC_POSTGRESQL_INTERNAL_HPP
#define CPP_DBC_POSTGRESQL_INTERNAL_HPP

#if USE_POSTGRESQL

#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstring>
#include "cpp_dbc/common/system_utils.hpp"

// Thread-safety macros for conditional mutex locking
// Using recursive_mutex to allow the same thread to acquire the lock multiple times
// This is needed when a method that holds the lock calls another method that also needs the lock
#if DB_DRIVER_THREAD_SAFE
#define DB_DRIVER_MUTEX mutable std::recursive_mutex
#define DB_DRIVER_LOCK_GUARD(mutex) std::scoped_lock<std::recursive_mutex> lock(mutex)
#define DB_DRIVER_UNIQUE_LOCK(mutex) std::unique_lock<std::recursive_mutex> lock(mutex)

// Macros for DBConnection methods - acquire lock and verify connection is not closed
// These are used in PostgreSQLDBConnection instead of plain DB_DRIVER_LOCK_GUARD

// For nothrow DBConnection methods - returns unexpected(DBException) if connection is closed
#define PG_CONNECTION_LOCK_OR_RETURN(mark, msg)                                                 \
    DB_DRIVER_LOCK_GUARD(m_connMutex);                                                         \
    if (m_closed.load(std::memory_order_acquire) || !m_conn)                                    \
    {                                                                                           \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",                \
                                               cpp_dbc::system_utils::captureCallStack()));     \
    }

// For throwing DBConnection methods - throws DBException if connection is closed
#define PG_CONNECTION_LOCK_OR_THROW(mark, msg)                            \
    DB_DRIVER_LOCK_GUARD(m_connMutex);                                   \
    if (m_closed.load(std::memory_order_acquire) || !m_conn)              \
    {                                                                     \
        throw DBException(mark, msg " (connection closed)",               \
                          cpp_dbc::system_utils::captureCallStack());     \
    }

// For close() method of DBConnection - returns success if already closed (idempotent)
#define PG_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                  \
    DB_DRIVER_LOCK_GUARD(m_connMutex);                                   \
    if (m_closed.load(std::memory_order_acquire) || !m_conn)              \
    {                                                                     \
        return {}; /* Already closed = success */                         \
    }

#else
#define DB_DRIVER_MUTEX
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#define DB_DRIVER_UNIQUE_LOCK(mutex) (void)0

// Non-thread-safe: still check closed state, no locking
#define PG_CONNECTION_LOCK_OR_RETURN(mark, msg)                                                 \
    if (m_closed.load(std::memory_order_acquire) || !m_conn)                                    \
    {                                                                                           \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",                \
                                               cpp_dbc::system_utils::captureCallStack()));     \
    }

#define PG_CONNECTION_LOCK_OR_THROW(mark, msg)                            \
    if (m_closed.load(std::memory_order_acquire) || !m_conn)              \
    {                                                                     \
        throw DBException(mark, msg " (connection closed)",               \
                          cpp_dbc::system_utils::captureCallStack());     \
    }

#define PG_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                  \
    if (m_closed.load(std::memory_order_acquire) || !m_conn)              \
    {                                                                     \
        return {};                                                        \
    }
#endif

// ============================================================================
// PostgreSQLConnectionLock - RAII Helper for PreparedStatement and ResultSet
// ============================================================================
// Automatically acquires the connection mutex through the weak_ptr<Connection>.
// If the connection is destroyed, marks the object as closed and fails gracefully.
// Usage:
//   PG_STMT_LOCK_OR_RETURN(error_code, "error message");
// ============================================================================
#if DB_DRIVER_THREAD_SAFE

namespace cpp_dbc::PostgreSQL
{
    // Forward declaration
    class PostgreSQLDBConnection;

    /**
     * @brief RAII lock helper that obtains mutex through weak_ptr<PostgreSQLDBConnection>
     *
     * This class implements the requirement that PreparedStatement and ResultSet
     * access the global lock ONLY through their parent DBConnection weak_ptr.
     *
     * If the connection is destroyed (weak_ptr expired), it automatically marks
     * the object as closed instead of crashing.
     *
     * IMPORTANT: This class keeps the DBConnection alive via shared_ptr while the lock
     * is held. This is necessary because the mutex itself is owned by
     * DBConnection, and we need to prevent the mutex from being destroyed while locked.
     */
    class PostgreSQLConnectionLock
    {
        std::shared_ptr<PostgreSQLDBConnection> m_conn; // Keep connection alive while lock is held
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
        PostgreSQLConnectionLock(T *obj, std::atomic<bool> &closed)
        {
            // FIRST CHECK: Verify if object is already closed (fast path, no lock needed)
            if (closed.load(std::memory_order_acquire))
            {
                m_acquired = false;
                return;
            }

            // Try to obtain connection from weak_ptr and KEEP IT ALIVE
            m_conn = obj->m_connection.lock();
            if (!m_conn)
            {
                // Connection is destroyed - mark object as closed
                closed.store(true, std::memory_order_release);
                m_acquired = false;
                return;
            }

            // Acquire the connection's mutex (connection stays alive via m_conn)
            m_lock = std::unique_lock<std::recursive_mutex>(m_conn->getConnectionMutex());

            // SECOND CHECK: Verify again after acquiring lock
            // (Another thread could have closed the object between first check and lock acquisition)
            if (closed.load(std::memory_order_acquire))
            {
                m_acquired = false;
                // Lock is released automatically by m_lock destructor
                // Connection will be released when m_conn goes out of scope
                return;
            }

            m_acquired = true;
        }

        bool isAcquired() const { return m_acquired; }
        explicit operator bool() const { return m_acquired; }
    };

} // namespace cpp_dbc::PostgreSQL

// Macro for nothrow methods - returns unexpected(DBException) if lock fails
#define PG_STMT_LOCK_OR_RETURN(mark, msg)                                                       \
    cpp_dbc::PostgreSQL::PostgreSQLConnectionLock __lock(this, m_closed);                        \
    if (!__lock)                                                                                \
    {                                                                                           \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",                \
                                               cpp_dbc::system_utils::captureCallStack()));     \
    }

// Macro for throwing methods - throws DBException if lock fails
#define PG_STMT_LOCK_OR_THROW(mark, msg)                                                                    \
    cpp_dbc::PostgreSQL::PostgreSQLConnectionLock __lock(this, m_closed);                                    \
    if (!__lock)                                                                                             \
    {                                                                                                        \
        throw DBException(mark, msg " (connection closed)", cpp_dbc::system_utils::captureCallStack());      \
    }

// Macro for close() methods - returns success if already closed (idempotent close)
#define PG_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                                              \
    cpp_dbc::PostgreSQL::PostgreSQLConnectionLock __lock(this, m_closed);                        \
    if (!__lock)                                                                                \
    {                                                                                           \
        return {}; /* Already closed or connection lost = success */                             \
    }

#else
#define PG_STMT_LOCK_OR_RETURN(mark, msg)                                                       \
    if (m_closed.load(std::memory_order_acquire))                                               \
    {                                                                                           \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",                \
                                               cpp_dbc::system_utils::captureCallStack()));     \
    }

#define PG_STMT_LOCK_OR_THROW(mark, msg)                                                                    \
    if (m_closed.load(std::memory_order_acquire))                                                            \
    {                                                                                                        \
        throw DBException(mark, msg " (connection closed)", cpp_dbc::system_utils::captureCallStack());      \
    }

#define PG_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                                              \
    if (m_closed.load(std::memory_order_acquire))                                               \
    {                                                                                           \
        return {}; /* Already closed or connection lost = success */                             \
    }
#endif // DB_DRIVER_THREAD_SAFE

// Debug output is controlled by -DDEBUG_POSTGRESQL=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_POSTGRESQL) && DEBUG_POSTGRESQL) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define PG_DEBUG(format, ...)                                                                     \
    do                                                                                            \
    {                                                                                             \
        char debug_buffer[1024];                                                                  \
        int pg_debug_n = std::snprintf(debug_buffer, sizeof(debug_buffer), format, ##__VA_ARGS__);\
        if (pg_debug_n < 0)                                                                       \
        {                                                                                         \
            static constexpr const char pg_fallback[] = "PostgreSQL debug formatting failed";     \
            std::memcpy(debug_buffer, pg_fallback, sizeof(pg_fallback));                          \
        }                                                                                         \
        else if (pg_debug_n >= static_cast<int>(sizeof(debug_buffer)))                            \
        {                                                                                         \
            static constexpr const char pg_trunc[] = "...[TRUNCATED]";                            \
            std::memcpy(debug_buffer + sizeof(debug_buffer) - sizeof(pg_trunc),                   \
                        pg_trunc, sizeof(pg_trunc));                                              \
        }                                                                                         \
        cpp_dbc::system_utils::logWithTimesMillis("PostgreSQL", debug_buffer);                    \
    } while (0)
#else
#define PG_DEBUG(...) ((void)0)
#endif

#endif // USE_POSTGRESQL

#endif // CPP_DBC_POSTGRESQL_INTERNAL_HPP
