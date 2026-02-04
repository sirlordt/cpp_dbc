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
 * @file handles.hpp
 * @brief RAII handle types for Cassandra/ScyllaDB C driver objects
 */

#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_SCYLLADB

#include <cassandra.h>
#include <memory>

namespace cpp_dbc::ScyllaDB
{
        /**
         * @brief RAII handle types for Cassandra/ScyllaDB C driver objects
         *
         * These custom deleters and type aliases provide automatic resource
         * management for the Cassandra C driver opaque pointer types using
         * `std::unique_ptr` and `std::shared_ptr` with custom deleters.
         *
         * ```cpp
         * // Handles are used internally by the ScyllaDB driver classes.
         * // Example: creating a session handle with automatic cleanup
         * CassSessionHandle session(cass_session_new(), CassSessionDeleter{});
         * CassFutureHandle future(cass_session_connect(session.get(), cluster),
         *                         CassFutureDeleter{});
         * ```
         *
         * @see ScyllaDBConnection, ScyllaDBPreparedStatement, ScyllaDBResultSet
         */

        /** @brief Custom deleter for CassCluster - calls cass_cluster_free() */
        struct CassClusterDeleter
        {
            void operator()(CassCluster *ptr) const
            {
                if (ptr)
                    cass_cluster_free(ptr);
            }
        };

        /** @brief Custom deleter for CassSession - calls cass_session_free() */
        struct CassSessionDeleter
        {
            void operator()(CassSession *ptr) const
            {
                if (ptr)
                    cass_session_free(ptr);
            }
        };

        /** @brief Custom deleter for CassFuture - calls cass_future_free() */
        struct CassFutureDeleter
        {
            void operator()(CassFuture *ptr) const
            {
                if (ptr)
                    cass_future_free(ptr);
            }
        };

        /** @brief Custom deleter for CassStatement - calls cass_statement_free() */
        struct CassStatementDeleter
        {
            void operator()(CassStatement *ptr) const
            {
                if (ptr)
                    cass_statement_free(ptr);
            }
        };

        /** @brief Custom deleter for CassPrepared - calls cass_prepared_free() */
        struct CassPreparedDeleter
        {
            void operator()(const CassPrepared *ptr) const
            {
                if (ptr)
                    cass_prepared_free(ptr);
            }
        };

        /** @brief Custom deleter for CassResult - calls cass_result_free() */
        struct CassResultDeleter
        {
            void operator()(const CassResult *ptr) const
            {
                if (ptr)
                    cass_result_free(ptr);
            }
        };

        /** @brief Custom deleter for CassIterator - calls cass_iterator_free() */
        struct CassIteratorDeleter
        {
            void operator()(CassIterator *ptr) const
            {
                if (ptr)
                    cass_iterator_free(ptr);
            }
        };

        /** @brief RAII handle for CassCluster using unique_ptr with custom deleter */
        using CassClusterHandle = std::unique_ptr<CassCluster, CassClusterDeleter>;
        /** @brief RAII handle for CassSession using unique_ptr with custom deleter */
        using CassSessionHandle = std::unique_ptr<CassSession, CassSessionDeleter>;
        /** @brief RAII handle for CassFuture using unique_ptr with custom deleter */
        using CassFutureHandle = std::unique_ptr<CassFuture, CassFutureDeleter>;
        /** @brief RAII handle for CassStatement using unique_ptr with custom deleter */
        using CassStatementHandle = std::unique_ptr<CassStatement, CassStatementDeleter>;
        /** @brief RAII handle for CassPrepared using shared_ptr (multiple statements can share one prepared) */
        using CassPreparedHandle = std::shared_ptr<const CassPrepared>; // Shared because multiple statements can be created from one prepared
        /** @brief RAII handle for CassResult using unique_ptr with custom deleter */
        using CassResultHandle = std::unique_ptr<const CassResult, CassResultDeleter>;

        /**
         * @brief Factory function to create a CassPreparedHandle with the correct deleter
         *
         * Since std::shared_ptr uses type-erased deleters (passed at construction, not
         * as a template parameter), this factory ensures CassPreparedDeleter is always
         * used, preventing accidental construction with the default `delete` operator.
         *
         * @param prepared Raw pointer to the CassPrepared object (takes ownership)
         * @return CassPreparedHandle with CassPreparedDeleter
         */
        inline CassPreparedHandle makeCassPreparedHandle(const CassPrepared *prepared)
        {
            return CassPreparedHandle(prepared, CassPreparedDeleter());
        }
        /** @brief RAII handle for CassIterator using unique_ptr with custom deleter */
        using CassIteratorHandle = std::unique_ptr<CassIterator, CassIteratorDeleter>;
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
