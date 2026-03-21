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
 * @file mongodb_internal.hpp
 * @brief MongoDB driver internal utilities - not part of public API
 */

#ifndef CPP_DBC_MONGODB_INTERNAL_HPP
#define CPP_DBC_MONGODB_INTERNAL_HPP

#if USE_MONGODB

#include <cstdio>
#include <cstring>
#include <mutex>

#include "cpp_dbc/common/system_utils.hpp"

// Thread-safety macros for conditional mutex locking
// Using recursive_mutex to allow the same thread to acquire the lock multiple times
#if DB_DRIVER_THREAD_SAFE
#define MONGODB_MUTEX mutable std::recursive_mutex
#define MONGODB_LOCK_GUARD(mutex) std::lock_guard<std::recursive_mutex> lock(mutex)
#define MONGODB_UNIQUE_LOCK(mutex) std::unique_lock<std::recursive_mutex> lock(mutex)
#else
// Dummy mutex type for when thread safety is disabled
// Allows MONGODB_MUTEX member declarations to remain well-formed
namespace cpp_dbc::MongoDB
{
    struct DummyRecursiveMutex
    {
        // No-op methods: thread safety disabled
        void lock() const noexcept { /* No-op */ }
        void unlock() const noexcept { /* No-op */ }
        bool try_lock() const noexcept { return true; }
    };
} // namespace cpp_dbc::MongoDB
#define MONGODB_MUTEX mutable cpp_dbc::MongoDB::DummyRecursiveMutex
#define MONGODB_LOCK_GUARD(mutex) std::lock_guard<cpp_dbc::MongoDB::DummyRecursiveMutex> lock(mutex)
#define MONGODB_UNIQUE_LOCK(mutex) std::unique_lock<cpp_dbc::MongoDB::DummyRecursiveMutex> lock(mutex)
#endif

// Debug output is controlled by -DDEBUG_MONGODB=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_MONGODB) && DEBUG_MONGODB) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define MONGODB_DEBUG(format, ...)                                                           \
    do                                                                                       \
    {                                                                                        \
        char debug_buffer[1024];                                                             \
        int debug_n = std::snprintf(debug_buffer, sizeof(debug_buffer), format, ##__VA_ARGS__); \
        if (debug_n >= static_cast<int>(sizeof(debug_buffer)))                               \
        {                                                                                    \
            static constexpr const char trunc[] = "...[TRUNCATED]";                          \
            std::memcpy(debug_buffer + sizeof(debug_buffer) - sizeof(trunc),                 \
                        trunc, sizeof(trunc));                                               \
        }                                                                                    \
        cpp_dbc::system_utils::logWithTimesMillis("MongoDB", debug_buffer);                  \
    } while (0)
#else
#define MONGODB_DEBUG(...) ((void)0)
#endif

// ============================================================================
// Connection-Level Macros — used inside MongoDBConnection methods
// ============================================================================

#if DB_DRIVER_THREAD_SAFE

#define MONGODB_CONNECTION_LOCK_OR_RETURN(mark, msg)                                        \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                                                     \
    if (m_closed.load(std::memory_order_seq_cst) || !m_client)                              \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

#define MONGODB_CONNECTION_LOCK_OR_THROW(mark, msg)                   \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                               \
    if (m_closed.load(std::memory_order_seq_cst) || !m_client)        \
    {                                                                 \
        throw DBException(mark, msg " (connection closed)",           \
                          cpp_dbc::system_utils::captureCallStack()); \
    }

#define MONGODB_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()  \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                        \
    if (m_closed.load(std::memory_order_seq_cst) || !m_client) \
    {                                                          \
        return {}; /* Already closed = success */              \
    }

#else // !DB_DRIVER_THREAD_SAFE

#define MONGODB_CONNECTION_LOCK_OR_RETURN(mark, msg)                                        \
    if (m_closed.load(std::memory_order_seq_cst) || !m_client)                              \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

#define MONGODB_CONNECTION_LOCK_OR_THROW(mark, msg)                   \
    if (m_closed.load(std::memory_order_seq_cst) || !m_client)        \
    {                                                                 \
        throw DBException(mark, msg " (connection closed)",           \
                          cpp_dbc::system_utils::captureCallStack()); \
    }

#define MONGODB_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()  \
    if (m_closed.load(std::memory_order_seq_cst) || !m_client) \
    {                                                          \
        return {}; /* Already closed = success */              \
    }

#endif // DB_DRIVER_THREAD_SAFE

// ============================================================================
// MongoDBConnectionLock — RAII helper for child objects (Collection, Cursor)
// ============================================================================

#if DB_DRIVER_THREAD_SAFE

namespace cpp_dbc::MongoDB
{
    // Forward declaration
    class MongoDBConnection;

    /**
     * @brief RAII lock helper that obtains mutex through weak_ptr<MongoDBConnection>
     *
     * Used by child objects (MongoDBCollection, MongoDBCursor) to acquire the
     * connection's mutex safely. Keeps the connection alive via shared_ptr while
     * the lock is held.
     */
    class MongoDBConnectionLock
    {
        std::shared_ptr<MongoDBConnection> m_conn;
        std::unique_lock<std::recursive_mutex> m_lock;
        bool m_acquired{false};

    public:
        template <typename T>
        MongoDBConnectionLock(T *obj, std::atomic<bool> &closed)
        {
            // FIRST CHECK: fast path — already closed, no lock needed
            if (closed.load(std::memory_order_seq_cst))
            {
                return;
            }

            // Obtain connection and KEEP IT ALIVE
            m_conn = obj->m_connection.lock();
            if (!m_conn)
            {
                closed.store(true, std::memory_order_seq_cst);
                return;
            }

            // Acquire mutex (connection stays alive via m_conn)
            m_lock = std::unique_lock<std::recursive_mutex>(m_conn->getConnectionMutex(std::nothrow));

            // SECOND CHECK: another thread may have closed between first check and lock
            if (closed.load(std::memory_order_seq_cst))
            {
                return;
            }

            // THIRD CHECK: parent connection explicitly closed
            if (m_conn->m_closed.load(std::memory_order_seq_cst))
            {
                closed.store(true, std::memory_order_seq_cst);
                return;
            }

            m_acquired = true;
        }

        bool isAcquired() const noexcept { return m_acquired; }
        explicit operator bool() const noexcept { return m_acquired; }

        /**
         * @brief Access the native mongoc_client_t* through the held connection
         * @return The native client pointer, or nullptr if lock not acquired
         */
        mongoc_client_t *nativeClient() const noexcept;
    };

} // namespace cpp_dbc::MongoDB

// Statement/Child-level macros — used inside Collection, Cursor, etc.

#define MONGODB_STMT_LOCK_OR_RETURN(mark, msg)                                              \
    cpp_dbc::MongoDB::MongoDBConnectionLock mongodb_conn_lock_(this, m_closed);              \
    if (!mongodb_conn_lock_)                                                                \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

#define MONGODB_STMT_LOCK_OR_THROW(mark, msg)                                               \
    cpp_dbc::MongoDB::MongoDBConnectionLock mongodb_conn_lock_(this, m_closed);              \
    if (!mongodb_conn_lock_)                                                                \
    {                                                                                       \
        throw DBException(mark, msg " (connection closed)",                                 \
                          cpp_dbc::system_utils::captureCallStack());                       \
    }

#define MONGODB_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                                     \
    cpp_dbc::MongoDB::MongoDBConnectionLock mongodb_conn_lock_(this, m_closed);              \
    if (!mongodb_conn_lock_)                                                                \
    {                                                                                       \
        return {}; /* Already closed or connection lost = success */                        \
    }

#else // !DB_DRIVER_THREAD_SAFE

// Non-thread-safe variants — check m_closed and m_connection.expired()

#define MONGODB_STMT_LOCK_OR_RETURN(mark, msg)                                              \
    if (m_closed.load(std::memory_order_seq_cst))                                           \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }                                                                                       \
    {                                                                                       \
        auto mongodb_conn_check_ = m_connection.lock();                                     \
        if (!mongodb_conn_check_ ||                                                         \
            mongodb_conn_check_->m_closed.load(std::memory_order_seq_cst))                  \
        {                                                                                   \
            m_closed.store(true, std::memory_order_seq_cst);                                \
            return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",        \
                                                   cpp_dbc::system_utils::captureCallStack())); \
        }                                                                                   \
    }

#define MONGODB_STMT_LOCK_OR_THROW(mark, msg)                                               \
    if (m_closed.load(std::memory_order_seq_cst))                                           \
    {                                                                                       \
        throw DBException(mark, msg " (connection closed)",                                 \
                          cpp_dbc::system_utils::captureCallStack());                       \
    }                                                                                       \
    {                                                                                       \
        auto mongodb_conn_check_ = m_connection.lock();                                     \
        if (!mongodb_conn_check_ ||                                                         \
            mongodb_conn_check_->m_closed.load(std::memory_order_seq_cst))                  \
        {                                                                                   \
            m_closed.store(true, std::memory_order_seq_cst);                                \
            throw DBException(mark, msg " (connection closed)",                             \
                              cpp_dbc::system_utils::captureCallStack());                   \
        }                                                                                   \
    }

#define MONGODB_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()          \
    if (m_closed.load(std::memory_order_seq_cst))                \
    {                                                            \
        return {}; /* Already closed = success */                \
    }                                                            \
    {                                                            \
        auto mongodb_conn_check_ = m_connection.lock();          \
        if (!mongodb_conn_check_ ||                              \
            mongodb_conn_check_->m_closed.load(                  \
                std::memory_order_seq_cst))                      \
        {                                                        \
            m_closed.store(true, std::memory_order_seq_cst);     \
            return {}; /* Connection closed = success */         \
        }                                                        \
    }

#endif // DB_DRIVER_THREAD_SAFE

#endif // USE_MONGODB

#endif // CPP_DBC_MONGODB_INTERNAL_HPP
