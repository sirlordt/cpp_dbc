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

#include <mutex>
#include <iostream>
#include <cstdio>
#include "cpp_dbc/common/system_utils.hpp"

// Thread-safety macros for conditional mutex locking
// Using recursive_mutex to allow the same thread to acquire the lock multiple times
// This is needed when a method that holds the lock calls another method that also needs the lock
#if DB_DRIVER_THREAD_SAFE
#define DB_DRIVER_MUTEX mutable std::recursive_mutex
#define DB_DRIVER_LOCK_GUARD(mutex) std::lock_guard<std::recursive_mutex> lock(mutex)
#define DB_DRIVER_UNIQUE_LOCK(mutex) std::unique_lock<std::recursive_mutex> lock(mutex)
#else
#define DB_DRIVER_MUTEX
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#define DB_DRIVER_UNIQUE_LOCK(mutex) (void)0
#endif

// SQLite uses m_globalFileMutex (file-level mutex from FileMutexRegistry) instead of
// m_connMutex. The lock is always acquired regardless of DB_DRIVER_THREAD_SAFE because
// the file mutex exists for sanitizer compatibility and cross-connection synchronization.

// For nothrow SQLiteDBConnection methods — locks file mutex and returns unexpected if closed
// Guard: if m_globalFileMutex was never initialized (constructor failed before mutex init),
// there's nothing to do — m_db is also null and the connection is in a failed state.
#define SQLITE_CONNECTION_LOCK_OR_RETURN(mark, msg)                                         \
    if (!m_globalFileMutex)                                                                 \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection not initialized)",   \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }                                                                                       \
    std::scoped_lock lock(*m_globalFileMutex);                                              \
    if (m_closed.load(std::memory_order_seq_cst) || !m_db)                                  \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",            \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }

// For close() — locks file mutex and returns success if already closed (idempotent)
// Guard: if m_globalFileMutex was never initialized, nothing to close.
#define SQLITE_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                                \
    if (!m_globalFileMutex)                                                                 \
    {                                                                                       \
        return {}; /* Mutex never initialized = nothing to close */                         \
    }                                                                                       \
    std::scoped_lock lock(*m_globalFileMutex);                                              \
    if (m_closed.load(std::memory_order_seq_cst) || !m_db)                                  \
    {                                                                                       \
        return {}; /* Already closed = success (idempotent) */                              \
    }

// For nothrow SQLiteDBPreparedStatement methods — locks file mutex and returns unexpected if closed
// Uses m_stmt instead of m_db since PreparedStatement owns a statement handle, not a db handle.
// Guard: if m_globalFileMutex was never initialized (constructor failed), nothing to do.
#define SQLITE_STMT_LOCK_OR_RETURN(mark, msg)                                               \
    if (!m_globalFileMutex)                                                                 \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (statement not initialized)",    \
                                               cpp_dbc::system_utils::captureCallStack())); \
    }                                                                                       \
    std::scoped_lock lock(*m_globalFileMutex);                                              \
    if (m_closed.load(std::memory_order_seq_cst) || !m_stmt)                                \
    {                                                                                       \
        return cpp_dbc::unexpected(DBException(mark, msg " (statement closed)",             \
                                               cpp_dbc::system_utils::captureCallStack())); \
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
            static constexpr const char sq_trunc[] = "...[TRUNCATED]";              \
            std::memcpy(debug_buffer + sizeof(debug_buffer) - sizeof(sq_trunc),      \
                        sq_trunc, sizeof(sq_trunc));                                  \
        }                                                                             \
        cpp_dbc::system_utils::logWithTimesMillis("SQLite", debug_buffer);            \
    } while (0)
#else
#define SQLITE_DEBUG(...) ((void)0)
#endif

#endif // USE_SQLITE

#endif // CPP_DBC_SQLITE_INTERNAL_HPP
