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
 * @file db_exception.hpp
 * @brief Exception classes for the cpp_dbc library
 */

#ifndef CPP_DBC_CORE_DB_EXCEPTION_HPP
#define CPP_DBC_CORE_DB_EXCEPTION_HPP

#include <stdexcept>
#include <string>
#include <vector>
#include "cpp_dbc/common/system_utils.hpp"

namespace cpp_dbc
{

    /**
     * @brief Base exception class for all database-related errors
     *
     * This exception provides:
     * - A mark/tag to identify the source of the error
     * - The error message
     * - Optional call stack information for debugging
     */
    class DBException : public std::runtime_error
    {
    private:
        std::string m_mark;
        mutable std::string m_full_message;
        std::vector<system_utils::StackFrame> m_callstack;

    public:
        /**
         * @brief Construct a new DBException
         *
         * @param mark A tag identifying the source of the error (e.g., "MySQL", "PostgreSQL")
         * @param message The error message
         * @param callstack Optional call stack for debugging
         */
        explicit DBException(const std::string &mark, const std::string &message,
                             const std::vector<system_utils::StackFrame> &callstack = {})
            : std::runtime_error(message),
              m_mark(mark),
              m_callstack(callstack) {}

        ~DBException() override = default;

        /**
         * @brief Get the error message as a C-string
         * @deprecated Use what_s() instead. It avoids the unsafe const char* pointer.
         */
        [[deprecated("Use what_s() instead. It avoids the unsafe const char* pointer.")]]
        const char *what() const noexcept override
        {
            if (m_mark.empty())
            {
                return std::runtime_error::what();
            }

            m_full_message = m_mark + ": " + std::runtime_error::what();
            return m_full_message.c_str();
        }

        /**
         * @brief Get the error message as a string reference (safe version)
         * @return const std::string& The full error message including the mark
         */
        virtual const std::string &what_s() const noexcept
        {
            if (m_mark.empty())
            {
                m_full_message = std::runtime_error::what();
                return m_full_message;
            }

            m_full_message = m_mark + ": " + std::runtime_error::what();
            return m_full_message;
        }

        /**
         * @brief Get the mark/tag identifying the error source
         */
        const std::string &getMark() const
        {
            return m_mark;
        }

        /**
         * @brief Print the call stack to stderr
         */
        void printCallStack() const
        {
            system_utils::printCallStack(m_callstack);
        }

        /**
         * @brief Get the call stack frames
         */
        const std::vector<system_utils::StackFrame> &getCallStack() const
        {
            return m_callstack;
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_EXCEPTION_HPP