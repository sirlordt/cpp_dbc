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
 * @file scylladb_internal.hpp
 * @brief ScyllaDB driver internal utilities - not part of public API
 */

#ifndef CPP_DBC_SCYLLADB_INTERNAL_HPP
#define CPP_DBC_SCYLLADB_INTERNAL_HPP

#if USE_SCYLLADB

#include <cassandra.h>
#include <mutex>
#include <iostream>

// Thread-safety macros for conditional mutex locking
#if DB_DRIVER_THREAD_SAFE
#define DB_DRIVER_MUTEX mutable std::recursive_mutex
#define DB_DRIVER_LOCK_GUARD(mutex) std::lock_guard<std::recursive_mutex> lock(mutex)
#define DB_DRIVER_UNIQUE_LOCK(mutex) std::unique_lock<std::recursive_mutex> lock(mutex)
#else
#define DB_DRIVER_MUTEX
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#define DB_DRIVER_UNIQUE_LOCK(mutex) (void)0
#endif

// Debug output is controlled by -DDEBUG_SCYLLADB=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_SCYLLADB) && DEBUG_SCYLLADB) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define SCYLLADB_DEBUG(x) std::cout << "[ScyllaDB] " << x << std::endl
#else
#define SCYLLADB_DEBUG(x)
#endif

namespace cpp_dbc::ScyllaDB
{
    // Helper to get value from column (used by ResultSet methods)
    inline const CassValue *get_column_value(const CassRow *row, size_t index, size_t count)
    {
        if (index >= count)
        {
            return nullptr;
        }
        return cass_row_get_column(row, index);
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB

#endif // CPP_DBC_SCYLLADB_INTERNAL_HPP
