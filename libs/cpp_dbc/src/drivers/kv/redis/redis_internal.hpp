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
 * @file redis_internal.hpp
 * @brief Redis driver internal utilities - not part of public API
 */

#ifndef CPP_DBC_REDIS_INTERNAL_HPP
#define CPP_DBC_REDIS_INTERNAL_HPP

#if USE_REDIS

#include <cstdio>
#include <cstring>
#include <mutex>

#include "cpp_dbc/common/system_utils.hpp"

// Thread-safety macros for conditional mutex locking
// Using recursive_mutex to allow the same thread to acquire the lock multiple times
#if DB_DRIVER_THREAD_SAFE
#define DB_DRIVER_LOCK_GUARD(mutex) std::scoped_lock<std::recursive_mutex> lock(mutex)
#else
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#endif

// Debug output is controlled by -DDEBUG_REDIS=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_REDIS) && DEBUG_REDIS) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define REDIS_DEBUG(format, ...)                                                             \
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
        cpp_dbc::system_utils::logWithTimesMillis("Redis", debug_buffer);                    \
    } while (0)
#else
#define REDIS_DEBUG(...) ((void)0)
#endif

// ============================================================================
// Connection-Level Macros — used inside RedisDBConnection methods
// ============================================================================

#if DB_DRIVER_THREAD_SAFE

#define REDIS_CONNECTION_LOCK_OR_RETURN(mark, msg)                                           \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                                                      \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)                                 \
    {                                                                                        \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",             \
                                               cpp_dbc::system_utils::captureCallStack()));  \
    }

#define REDIS_CONNECTION_LOCK_OR_THROW(mark, msg)                    \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                              \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)         \
    {                                                                \
        throw DBException(mark, msg " (connection closed)",          \
                          cpp_dbc::system_utils::captureCallStack());\
    }

#define REDIS_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()   \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                       \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)  \
    {                                                         \
        return {}; /* Already closed = success */             \
    }

#else // !DB_DRIVER_THREAD_SAFE

#define REDIS_CONNECTION_LOCK_OR_RETURN(mark, msg)                                           \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)                                 \
    {                                                                                        \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",             \
                                               cpp_dbc::system_utils::captureCallStack()));  \
    }

#define REDIS_CONNECTION_LOCK_OR_THROW(mark, msg)                    \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)         \
    {                                                                \
        throw DBException(mark, msg " (connection closed)",          \
                          cpp_dbc::system_utils::captureCallStack());\
    }

#define REDIS_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()   \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)  \
    {                                                         \
        return {}; /* Already closed = success */             \
    }

#endif // DB_DRIVER_THREAD_SAFE

#endif // USE_REDIS

#endif // CPP_DBC_REDIS_INTERNAL_HPP
