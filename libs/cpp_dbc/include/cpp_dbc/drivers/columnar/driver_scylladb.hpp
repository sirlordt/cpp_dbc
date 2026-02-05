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
 * @file driver_scylladb.hpp
 * @brief Umbrella header for the ScyllaDB (Cassandra) columnar database driver
 *
 * Include this single header to access all ScyllaDB driver types:
 * ScyllaDBDriver, ScyllaDBConnection, ScyllaDBPreparedStatement,
 * ScyllaDBResultSet, ScyllaMemoryInputStream, and RAII handle types.
 */

#ifndef CPP_DBC_DRIVER_SCYLLA_HPP
#define CPP_DBC_DRIVER_SCYLLA_HPP

#include "scylladb/handles.hpp"
#include "scylladb/memory_input_stream.hpp"
#include "scylladb/result_set.hpp"
#include "scylladb/prepared_statement.hpp"
#include "scylladb/connection.hpp"
#include "scylladb/driver.hpp"

#endif // CPP_DBC_DRIVER_SCYLLA_HPP
