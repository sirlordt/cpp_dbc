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

#include <mutex>
#include <iostream>

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
        void lock() const noexcept { /* No-op: thread safety disabled */ }
        void unlock() const noexcept { /* No-op: thread safety disabled */ }
        bool try_lock() const noexcept { return true; }
    };
} // namespace cpp_dbc::MongoDB
#define MONGODB_MUTEX mutable cpp_dbc::MongoDB::DummyRecursiveMutex
#define MONGODB_LOCK_GUARD(mutex) std::lock_guard<cpp_dbc::MongoDB::DummyRecursiveMutex> lock(mutex)
#define MONGODB_UNIQUE_LOCK(mutex) std::unique_lock<cpp_dbc::MongoDB::DummyRecursiveMutex> lock(mutex)
#endif

// Debug output is controlled by -DDEBUG_MONGODB=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_MONGODB) && DEBUG_MONGODB) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define MONGODB_DEBUG(x) std::cout << "[MongoDB] " << x << std::endl
#else
#define MONGODB_DEBUG(x)
#endif

#endif // USE_MONGODB

#endif // CPP_DBC_MONGODB_INTERNAL_HPP
