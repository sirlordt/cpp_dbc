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
 * @file mysql_internal.hpp
 * @brief MySQL driver internal utilities - not part of public API
 */

#ifndef CPP_DBC_MYSQL_INTERNAL_HPP
#define CPP_DBC_MYSQL_INTERNAL_HPP

#if USE_MYSQL

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
// These are used in MySQLDBConnection instead of plain DB_DRIVER_LOCK_GUARD

// For nothrow DBConnection methods - returns unexpected(DBException) if connection is closed
#define MYSQL_CONNECTION_LOCK_OR_RETURN(mark, msg)                                          \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                                                     \
    if (m_closed.load(std::memory_order_acquire))                                           \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

// For throwing DBConnection methods - throws DBException if connection is closed
#define MYSQL_CONNECTION_LOCK_OR_THROW(mark, msg)                     \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                               \
    if (m_closed.load(std::memory_order_acquire))                     \
    {                                                                 \
        throw DBException(mark, msg " (connection closed)",           \
                          cpp_dbc::system_utils::captureCallStack()); \
    }

// For close() method of DBConnection - returns success if already closed (idempotent)
#define MYSQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED() \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                     \
    if (m_closed.load(std::memory_order_acquire))           \
    {                                                       \
        return {}; /* Already closed = success */           \
    }

#else
#define DB_DRIVER_MUTEX
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#define DB_DRIVER_UNIQUE_LOCK(mutex) (void)0
#define MYSQL_CONNECTION_LOCK_OR_RETURN(mark, msg)                                          \
    if (m_closed.load(std::memory_order_acquire))                                           \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

#define MYSQL_CONNECTION_LOCK_OR_THROW(mark, msg)                     \
    if (m_closed.load(std::memory_order_acquire))                     \
    {                                                                 \
        throw DBException(mark, msg " (connection closed)",           \
                          cpp_dbc::system_utils::captureCallStack()); \
    }

#define MYSQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED() \
    if (m_closed.load(std::memory_order_acquire))           \
    {                                                       \
        return {};                                          \
    }
#endif

// ============================================================================
// MySQLConnectionLock - RAII Helper for PreparedStatement and ResultSet
// ============================================================================
// Automatically acquires the connection mutex through the weak_ptr<Connection>.
// If the connection is destroyed, marks the object as closed and fails gracefully.
// ============================================================================
#if DB_DRIVER_THREAD_SAFE

namespace cpp_dbc::MySQL
{
    // Forward declaration
    class MySQLDBConnection;

    /**
     * @brief RAII lock helper that obtains mutex through weak_ptr<MySQLDBConnection>
     *
     * Implements double-checked locking: check closed → lock connection → check closed again.
     * If the connection is destroyed (weak_ptr expired), automatically marks the object as closed.
     * Keeps the DBConnection alive via shared_ptr while the lock is held.
     */
    class MySQLConnectionLock
    {
        std::shared_ptr<MySQLDBConnection> m_conn;
        std::unique_lock<std::recursive_mutex> m_lock;
        bool m_acquired{false};

    public:
        /**
         * @brief Acquire lock through connection weak_ptr with double-checked locking
         * @param obj The object (PreparedStatement or ResultSet) that has m_connection member
         * @param closed Atomic flag to check/mark object closed state
         */
        template <typename T>
        MySQLConnectionLock(T *obj, std::atomic<bool> &closed)
        {
            // FIRST CHECK: fast path, no lock needed
            if (closed.load(std::memory_order_acquire))
            {
                m_acquired = false;
                return;
            }

            // Try to obtain connection from weak_ptr and KEEP IT ALIVE
            m_conn = obj->m_connection.lock();
            if (!m_conn)
            {
                // Connection is destroyed — mark object as closed
                closed.store(true, std::memory_order_release);
                m_acquired = false;
                return;
            }

            // Acquire the connection's mutex (connection stays alive via m_conn)
            m_lock = std::unique_lock<std::recursive_mutex>(m_conn->getConnectionMutex(std::nothrow));

            // SECOND CHECK: another thread could have closed between first check and lock
            if (closed.load(std::memory_order_acquire))
            {
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

} // namespace cpp_dbc::MySQL

// Macro for nothrow PreparedStatement/ResultSet methods - returns unexpected if lock fails
#define MYSQL_STMT_LOCK_OR_RETURN(mark, msg)                                                \
    cpp_dbc::MySQL::MySQLConnectionLock __lock(this, m_closed);                             \
    if (!__lock)                                                                            \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

// Macro for throwing PreparedStatement/ResultSet methods - throws if lock fails
#define MYSQL_STMT_LOCK_OR_THROW(mark, msg)                                                             \
    cpp_dbc::MySQL::MySQLConnectionLock __lock(this, m_closed);                                         \
    if (!__lock)                                                                                        \
    {                                                                                                   \
        throw DBException(mark, msg " (connection closed)", cpp_dbc::system_utils::captureCallStack()); \
    }

// Macro for close() methods - returns success if already closed (idempotent)
#define MYSQL_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                \
    cpp_dbc::MySQL::MySQLConnectionLock __lock(this, m_closed);      \
    if (!__lock)                                                     \
    {                                                                \
        return {}; /* Already closed or connection lost = success */ \
    }

#else
// Non-thread-safe: just check the closed flag, no locking
#define MYSQL_STMT_LOCK_OR_RETURN(mark, msg)                                                \
    if (m_closed.load(std::memory_order_acquire))                                           \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

#define MYSQL_STMT_LOCK_OR_THROW(mark, msg)                                                             \
    if (m_closed.load(std::memory_order_acquire))                                                       \
    {                                                                                                   \
        throw DBException(mark, msg " (connection closed)", cpp_dbc::system_utils::captureCallStack()); \
    }

#define MYSQL_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED() \
    if (m_closed.load(std::memory_order_acquire))     \
    {                                                 \
        return {}; /* Already closed = success */     \
    }
#endif // DB_DRIVER_THREAD_SAFE

// Debug output is controlled by -DDEBUG_MYSQL=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_MYSQL) && DEBUG_MYSQL) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define MYSQL_DEBUG(format, ...)                                                                      \
    do                                                                                                \
    {                                                                                                 \
        char debug_buffer[1024];                                                                      \
        int mysql_debug_n = std::snprintf(debug_buffer, sizeof(debug_buffer), format, ##__VA_ARGS__); \
        if (mysql_debug_n >= static_cast<int>(sizeof(debug_buffer)))                                  \
        {                                                                                             \
            static constexpr const char mysql_trunc[] = "...[TRUNCATED]";                             \
            std::memcpy(debug_buffer + sizeof(debug_buffer) - sizeof(mysql_trunc),                    \
                        mysql_trunc, sizeof(mysql_trunc));                                            \
        }                                                                                             \
        cpp_dbc::system_utils::logWithTimesMillis("MySQL", debug_buffer);                             \
    } while (0)
#else
#define MYSQL_DEBUG(...) ((void)0)
#endif

#endif // USE_MYSQL

#endif // CPP_DBC_MYSQL_INTERNAL_HPP
