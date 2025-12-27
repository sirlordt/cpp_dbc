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

namespace cpp_dbc
{

    /**
     * @brief Abstract base class for all database result sets
     *
     * This is the root of the result set hierarchy. All database paradigms
     * (relational, document, key-value, columnar) derive their result set
     * classes from this base class.
     *
     * The base class provides only the most fundamental operations that
     * are common to all database types:
     * - close(): Release resources
     * - isEmpty(): Check if the result set contains any data
     */
    class DBResultSet
    {
    public:
        virtual ~DBResultSet() = default;

        /**
         * @brief Close the result set and release associated resources
         *
         * After calling close(), the result set should not be used.
         * Implementations should handle multiple calls to close() gracefully.
         */
        virtual void close() = 0;

        /**
         * @brief Check if the result set is empty
         *
         * @return true if the result set contains no data
         * @return false if the result set contains at least one row/document/value
         */
        virtual bool isEmpty() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_RESULT_SET_HPP