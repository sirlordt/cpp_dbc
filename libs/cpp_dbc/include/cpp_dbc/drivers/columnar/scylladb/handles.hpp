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
        // Custom deleters for Cassandra objects
        struct CassClusterDeleter
        {
            void operator()(CassCluster *ptr) const
            {
                if (ptr)
                    cass_cluster_free(ptr);
            }
        };

        struct CassSessionDeleter
        {
            void operator()(CassSession *ptr) const
            {
                if (ptr)
                    cass_session_free(ptr);
            }
        };

        struct CassFutureDeleter
        {
            void operator()(CassFuture *ptr) const
            {
                if (ptr)
                    cass_future_free(ptr);
            }
        };

        struct CassStatementDeleter
        {
            void operator()(CassStatement *ptr) const
            {
                if (ptr)
                    cass_statement_free(ptr);
            }
        };

        struct CassPreparedDeleter
        {
            void operator()(const CassPrepared *ptr) const
            {
                if (ptr)
                    cass_prepared_free(ptr);
            }
        };

        struct CassResultDeleter
        {
            void operator()(const CassResult *ptr) const
            {
                if (ptr)
                    cass_result_free(ptr);
            }
        };

        struct CassIteratorDeleter
        {
            void operator()(CassIterator *ptr) const
            {
                if (ptr)
                    cass_iterator_free(ptr);
            }
        };

        using CassClusterHandle = std::unique_ptr<CassCluster, CassClusterDeleter>;
        using CassSessionHandle = std::unique_ptr<CassSession, CassSessionDeleter>;
        using CassFutureHandle = std::unique_ptr<CassFuture, CassFutureDeleter>;
        using CassStatementHandle = std::unique_ptr<CassStatement, CassStatementDeleter>;
        using CassPreparedHandle = std::shared_ptr<const CassPrepared>; // Shared because multiple statements can be created from one prepared
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
        using CassIteratorHandle = std::unique_ptr<CassIterator, CassIteratorDeleter>;
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
