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
 * @file db_result_set.hpp
 * @brief Abstract base class for all database result sets
 */

#ifndef CPP_DBC_CORE_DB_RESULT_SET_HPP
#define CPP_DBC_CORE_DB_RESULT_SET_HPP

#include <new> // For std::nothrow_t
#include "db_expected.hpp"
#include "db_exception.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract base class for all database result sets
     *
     * Base class for paradigm-specific result sets. Use the appropriate subclass
     * (RelationalDBResultSet, ColumnarDBResultSet, etc.) for typed data access.
     *
     * ```cpp
     * auto relConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(conn);
     * auto rs = relConn->executeQuery("SELECT COUNT(*) FROM users");
     * if (!rs->isEmpty()) {
     *     rs->next();
     *     std::cout << "Count: " << rs->getInt(1) << std::endl;
     * }
     * rs->close();
     * ```
     *
     * @see RelationalDBResultSet, ColumnarDBResultSet
     */
    class DBResultSet
    {
    public:
        virtual ~DBResultSet() = default;

        /**
         * @brief Close the result set and release associated resources
         *
         * After calling close(), the result set should not be used.
         * Implementations handle multiple calls to close() gracefully.
         */
        #ifdef __cpp_exceptions
        virtual void close() = 0;

        /**
         * @brief Check if the result set is empty (contains no rows/documents)
         *
         * @return true if the result set contains no data
         * @return false if the result set contains at least one row/document/value
         */
        virtual bool isEmpty() = 0;

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Close the result set and release resources (nothrow version)
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept = 0;

        /**
         * @brief Check if the result set is empty (nothrow version)
         * @param std::nothrow_t Nothrow tag to indicate no-throw semantics
         * @return expected containing true if empty, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException> isEmpty(std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_RESULT_SET_HPP