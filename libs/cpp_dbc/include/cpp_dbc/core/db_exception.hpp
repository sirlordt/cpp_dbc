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

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include "cpp_dbc/common/system_utils.hpp"

// #define used for array dimensions inside DBException so IntelliSense sees preprocessor
// literals rather than static constexpr member references — which trigger a false-positive
// "expression must be a modifiable lvalue" when writing to the arrays via strncpy/snprintf.
// The static constexpr members (MARK_LEN, MESSAGE_MAX, FULL_MSG_MAX) remain the canonical
// type-safe API for all code outside the class. #undef'd after the class definition.
#define CPP_DBC_EXCEPTION_MARK_LEN     12
#define CPP_DBC_EXCEPTION_MESSAGE_MAX  256
// 12 (mark) + 2 (": ") + 256 (message)
#define CPP_DBC_EXCEPTION_FULL_MSG_MAX 270

namespace cpp_dbc
{

    /**
     * @brief Base exception class for all database-related errors
     *
     * Fixed-size value type: all string storage is in fixed-size char arrays.
     * The call stack is heap-allocated on demand via shared_ptr — DBException itself
     * stores only a 16-byte pointer. When no callstack is passed the pointer is
     * nullptr and no heap memory is allocated at all.
     *
     * Total object size is approximately 560 bytes (mark + message + full_message +
     * shared_ptr). The CallStackCapture (~3,040 bytes) lives on the heap only when
     * captureCallStack() is called.
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
    class DBException : public std::exception
    {
    public:
        /// Length of the fixed error code (always exactly 12 alphanumeric characters).
        static constexpr std::size_t MARK_LEN    = CPP_DBC_EXCEPTION_MARK_LEN;

        /// Maximum number of bytes (excluding null terminator) for the error message.
        /// Messages longer than this are right-truncated (prefix preserved).
        static constexpr std::size_t MESSAGE_MAX = CPP_DBC_EXCEPTION_MESSAGE_MAX;

        /// Maximum size of the pre-computed "MARK: message" string (excl. null terminator).
        static constexpr std::size_t FULL_MSG_MAX = CPP_DBC_EXCEPTION_FULL_MSG_MAX; // MARK_LEN + 2 + MESSAGE_MAX

    private:
        char m_mark[CPP_DBC_EXCEPTION_MARK_LEN + 1]{};
        char m_message[CPP_DBC_EXCEPTION_MESSAGE_MAX + 1]{};
        char m_full_message[CPP_DBC_EXCEPTION_FULL_MSG_MAX + 1]{}; // "XXXXXXXXXXXX: <msg>\0"

        std::shared_ptr<system_utils::CallStackCapture> m_callstack{};

    public:
        /**
         * @brief Construct a new DBException
         *
         * All string data is copied into fixed internal buffers. The constructor
         * is declared noexcept — the shared_ptr copy cannot throw because the
         * allocation already occurred inside captureCallStack().
         *
         * @param mark A unique 12-character alphanumeric error code
         * @param message The human-readable error message (truncated at MESSAGE_MAX chars)
         * @param callstack Optional call stack captured via system_utils::captureCallStack()
         *                  Pass nullptr (default) to skip the stack trace entirely.
         *
         * ```cpp
         * throw cpp_dbc::DBException("7K3F9J2B5Z8D",
         *     "Connection refused", system_utils::captureCallStack());
         * ```
         */
        explicit DBException(const std::string &mark,
                             const std::string &message,
                             std::shared_ptr<system_utils::CallStackCapture> callstack = nullptr) noexcept
            : m_callstack(std::move(callstack))
        {
            // Copy mark — always exactly MARK_LEN chars; null-terminate defensively
            std::strncpy(m_mark, mark.c_str(), MARK_LEN);
            m_mark[MARK_LEN] = '\0';

            // Copy message — right-truncated (prefix preserved) if message exceeds MESSAGE_MAX chars
            std::strncpy(m_message, message.c_str(), MESSAGE_MAX);
            m_message[MESSAGE_MAX] = '\0';

            // Pre-compute the combined string used by what() and what_s().
            // Done once here so what() is a trivial noexcept pointer return with no allocation.
            // When mark is empty the full message is just the error message (preserves the
            // original DBException behavior for backwards-compatible use with empty marks).
            if (m_mark[0] == '\0')
            {
                std::strncpy(m_full_message, m_message, FULL_MSG_MAX);
                m_full_message[FULL_MSG_MAX] = '\0';
            }
            else
            {
                std::snprintf(m_full_message, sizeof(m_full_message), "%s: %s", m_mark, m_message);
            }
        }

        ~DBException() override = default;

        // Default copy and move are correct: char arrays are trivially copyable and
        // shared_ptr copy/move only adjust the reference count — no deep allocation.
        DBException(const DBException &)            = default;
        DBException &operator=(const DBException &) = default;
        DBException(DBException &&)                 = default;
        DBException &operator=(DBException &&)      = default;

        /**
         * @brief Get the full error message as a C-string
         *
         * Returns the pre-computed "MARK: message" string. Never allocates.
         * @deprecated Prefer what_s() for safer string_view access.
         */
        // NOSONAR(cpp:S1133) — Required for std::exception compatibility
        const char *what() const noexcept override
        {
            return m_full_message;
        }

        /**
         * @brief Get the full error message as a safe string_view
         *
         * Returns a view of the pre-computed "MARK: message" string (e.g.,
         * "7K3F9J2B5Z8D: Connection refused"). Never allocates. The view is
         * valid for the lifetime of this DBException object.
         *
         * ```cpp
         * catch (const cpp_dbc::DBException &e) {
         *     std::cerr << e.what_s() << std::endl;
         * }
         * ```
         */
        virtual std::string_view what_s() const noexcept
        {
            return std::string_view{m_full_message};
        }

        /**
         * @brief Get the unique error code identifying this error
         *
         * @return string_view over the 12-character alphanumeric error code
         */
        std::string_view getMark() const noexcept
        {
            return std::string_view{m_mark};
        }

        /**
         * @brief Print the captured call stack to stdout
         *
         * No-op if no call stack was captured at throw time (nullptr).
         */
        void printCallStack() const
        {
            system_utils::printCallStack(m_callstack);
        }

        /**
         * @brief Get the raw call stack frames for programmatic access
         *
         * @return span over the captured StackFrame entries (empty if no stack was captured)
         */
        std::span<const system_utils::StackFrame> getCallStack() const noexcept
        {
            if (!m_callstack)
            {
                return {};
            }
            return std::span<const system_utils::StackFrame>{m_callstack->frames, m_callstack->count};
        }
    };

} // namespace cpp_dbc

#undef CPP_DBC_EXCEPTION_MARK_LEN
#undef CPP_DBC_EXCEPTION_MESSAGE_MAX
#undef CPP_DBC_EXCEPTION_FULL_MSG_MAX

#endif // CPP_DBC_CORE_DB_EXCEPTION_HPP
