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
 * @file connection_pool_internal.hpp
 * @brief Connection pool internal utilities - not part of public API
 *
 * Provides the CP_DEBUG macro shared by all connection pool implementations
 * (relational, document, etc.). The macro is thread-safe and delegates to
 * cpp_dbc::system_utils::logWithTimesMillis() which prepends [HH:MM:SS.mmm]
 * and [thread_id] automatically and serialises output with an internal mutex.
 *
 * Usage (printf-style, identical to FIREBIRD_DEBUG):
 *   CP_DEBUG("Pool::close - active connections: %zu", m_activeConnections.load());
 *   CP_DEBUG("Pool::returnConnection - exception: %s", ex.what());
 *
 * Enabled by compiling with -DDEBUG_CONNECTION_POOL=1 or -DDEBUG_ALL=1.
 */

#ifndef CPP_DBC_CONNECTION_POOL_INTERNAL_HPP
#define CPP_DBC_CONNECTION_POOL_INTERNAL_HPP

#include <cstdio>
#include "cpp_dbc/common/system_utils.hpp"

// Undefine any prior CP_DEBUG (e.g. the no-op from high_perf_logger.hpp) so that our
// logWithTimesMillis-based, DEBUG_CONNECTION_POOL-controlled version takes precedence.
#ifdef CP_DEBUG
#undef CP_DEBUG
#endif

// Debug output is controlled by -DDEBUG_CONNECTION_POOL=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_CONNECTION_POOL) && DEBUG_CONNECTION_POOL) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define CP_DEBUG(format, ...)                                                                    \
    do                                                                                           \
    {                                                                                            \
        char cp_debug_buf[1024];                                                                 \
        int cp_debug_n = std::snprintf(cp_debug_buf, sizeof(cp_debug_buf), format, ##__VA_ARGS__); \
        if (cp_debug_n >= static_cast<int>(sizeof(cp_debug_buf)))                               \
        {                                                                                        \
            /* Message was truncated — mark the end of the buffer to make it visible */          \
            static constexpr const char cp_trunc[] = "...[TRUNCATED]";                          \
            std::memcpy(cp_debug_buf + sizeof(cp_debug_buf) - sizeof(cp_trunc),                 \
                        cp_trunc, sizeof(cp_trunc));                                             \
        }                                                                                        \
        cpp_dbc::system_utils::logWithTimesMillis("ConnectionPool", cp_debug_buf);               \
    } while (0)
#else
#define CP_DEBUG(format, ...) ((void)0)
#endif

#endif // CPP_DBC_CONNECTION_POOL_INTERNAL_HPP
