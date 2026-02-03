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
 * @file driver_mongodb.hpp
 * @brief MongoDB database driver implementation using libmongoc (MongoDB C Driver)
 *
 * This driver provides a safe, thread-safe interface to MongoDB using:
 * - Smart pointers (shared_ptr, weak_ptr, unique_ptr) for memory management
 * - RAII patterns for resource cleanup
 * - Mutex-based thread safety
 * - Exception-safe operations
 *
 * Dependencies:
 * - libmongoc-1.0 (MongoDB C Driver)
 * - libbson-1.0 (BSON library)
 */

#ifndef CPP_DBC_DRIVER_MONGODB_HPP
#define CPP_DBC_DRIVER_MONGODB_HPP

#include "mongodb/handles.hpp"
#include "mongodb/document.hpp"
#include "mongodb/cursor.hpp"
#include "mongodb/collection.hpp"
#include "mongodb/connection.hpp"
#include "mongodb/driver.hpp"

#endif // CPP_DBC_DRIVER_MONGODB_HPP
