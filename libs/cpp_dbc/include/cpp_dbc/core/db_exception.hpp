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
     * Every error thrown by cpp_dbc is a DBException. It carries a unique
     * 12-character error code (mark), a human-readable message, and an
     * optional captured call stack for debugging.
     *
     * ```cpp
     * try {
     *     auto conn = cpp_dbc::DriverManager::getDBConnection(url, user, pass);
     *     conn->close();
     * } catch (const cpp_dbc::DBException &e) {
     *     std::cerr << "Error [" << e.getMark() << "]: " << e.what_s() << std::endl;
     *     e.printCallStack();
     * }
     * ```
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
         * @param mark A unique 12-character alphanumeric error code
         * @param message The human-readable error message
         * @param callstack Optional call stack captured via system_utils::captureCallStack()
         *
         * ```cpp
         * throw cpp_dbc::DBException("7K3F9J2B5Z8D",
         *     "Connection refused", system_utils::captureCallStack());
         * ```
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
        const char *what() const noexcept override // NOSONAR(cpp:S1133) - Required for std::exception compatibility
        {
            if (m_mark.empty())
            {
                return std::runtime_error::what();
            }

            m_full_message = m_mark + ": " + std::runtime_error::what();
            return m_full_message.c_str();
        }

        /**
         * @brief Get the full error message as a safe string reference
         *
         * Returns the mark and message combined (e.g., "7K3F9J2B5Z8D: Connection refused").
         *
         * @return const std::string& The full error message including the mark
         *
         * ```cpp
         * catch (const cpp_dbc::DBException &e) {
         *     std::cerr << e.what_s() << std::endl;
         * }
         * ```
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
         * @brief Get the unique error code identifying this error
         *
         * @return const std::string& The 12-character alphanumeric error code
         */
        const std::string &getMark() const
        {
            return m_mark;
        }

        /**
         * @brief Print the captured call stack to stderr
         *
         * Only produces output if a call stack was captured at throw time.
         */
        void printCallStack() const
        {
            system_utils::printCallStack(m_callstack);
        }

        /**
         * @brief Get the raw call stack frames for programmatic access
         *
         * @return const std::vector<system_utils::StackFrame>& The captured stack frames
         */
        const std::vector<system_utils::StackFrame> &getCallStack() const
        {
            return m_callstack;
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_EXCEPTION_HPP