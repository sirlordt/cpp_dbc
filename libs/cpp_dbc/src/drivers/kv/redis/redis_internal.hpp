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

#include <mutex>
#include <iostream>

// Debug output is controlled by -DDEBUG_REDIS=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_REDIS) && DEBUG_REDIS) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define REDIS_DEBUG(x) std::cout << "[Redis] " << x << std::endl
#else
#define REDIS_DEBUG(x)
#endif

// Macro for mutex lock guard
#define REDIS_LOCK_GUARD(mtx) std::lock_guard<std::mutex> lock_(mtx)

#endif // CPP_DBC_REDIS_INTERNAL_HPP
