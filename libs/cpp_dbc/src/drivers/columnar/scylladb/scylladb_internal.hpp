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
#include <string>
#include <algorithm>
#include <cctype>

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

    /**
     * @brief Estimates the number of affected rows for a CQL statement
     *
     * Cassandra/ScyllaDB does not provide affected row counts natively.
     * This function provides a best-effort heuristic based on query analysis.
     *
     * @param query The CQL query string to analyze
     * @return Estimated number of affected rows
     *
     * @note Limitations and edge cases:
     *   - DDL statements (CREATE, DROP, ALTER, TRUNCATE): Always returns 0
     *   - DELETE with "WHERE id IN (...)": Counts comma-separated values as affected rows.
     *     This is a heuristic and may be inaccurate if:
     *       - Some IDs don't exist in the database (over-counting)
     *       - The IN clause contains nested structures or expressions
     *       - The column is not named "id" (case-insensitive match on "ID")
     *   - DELETE/UPDATE/INSERT without IN clause: Returns 1 (assumes single row)
     *     This is inaccurate for:
     *       - Range deletes (e.g., WHERE timestamp > ...)
     *       - Partition-level deletes
     *       - Batch operations processed as single statements
     *   - Unknown statements: Returns 1 to indicate success
     *
     * @warning This function cannot determine actual affected rows. For accurate
     *          counts, consider using Lightweight Transactions (LWT) with IF EXISTS/
     *          IF NOT EXISTS and checking the [applied] column in the result.
     */
    inline uint64_t estimateAffectedRows(const std::string &query)
    {
        // Convert to uppercase for case-insensitive matching
        std::string queryUpper = query;
        std::ranges::transform(queryUpper, queryUpper.begin(), [](unsigned char c)
                               { return static_cast<char>(std::toupper(c)); });

        // DDL statements conventionally return 0 affected rows
        if (queryUpper.starts_with("CREATE ") ||
            queryUpper.starts_with("DROP ") ||
            queryUpper.starts_with("ALTER ") ||
            queryUpper.starts_with("TRUNCATE "))
        {
            SCYLLADB_DEBUG("estimateAffectedRows - DDL statement, returning 0");
            return 0;
        }

        // For DELETE operations
        if (queryUpper.starts_with("DELETE "))
        {
            // Special case for 'IN (...)' clause to handle multiple rows (any column)
            size_t inStart = queryUpper.find(" IN (");
            if (inStart != std::string::npos)
            {
                // Count the number of elements in the IN clause
                size_t parenPos = inStart + 4; // Position of '(' in " IN ("
                size_t inEnd = queryUpper.find(")", parenPos);
                if (inEnd != std::string::npos)
                {
                    std::string inClause = queryUpper.substr(parenPos + 1, inEnd - parenPos - 1);
                    size_t commaCount = 0;
                    for (char c : inClause)
                    {
                        if (c == ',')
                        {
                            commaCount++;
                        }
                    }
                    uint64_t count = commaCount + 1;
                    SCYLLADB_DEBUG("estimateAffectedRows - DELETE with IN clause, affected rows: " << count);
                    return count;
                }
            }
            SCYLLADB_DEBUG("estimateAffectedRows - DELETE operation, assuming 1 affected row");
            return 1;
        }

        // For UPDATE operations
        if (queryUpper.starts_with("UPDATE "))
        {
            SCYLLADB_DEBUG("estimateAffectedRows - UPDATE operation, assuming 1 affected row");
            return 1;
        }

        // For INSERT operations
        if (queryUpper.starts_with("INSERT "))
        {
            SCYLLADB_DEBUG("estimateAffectedRows - INSERT operation, assuming 1 affected row");
            return 1;
        }

        // For any other operations, return 1 to indicate success
        SCYLLADB_DEBUG("estimateAffectedRows - Other operation, returning 1");
        return 1;
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB

#endif // CPP_DBC_SCYLLADB_INTERNAL_HPP
