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

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include "cpp_dbc/common/system_utils.hpp"

// #define used for array dimensions inside DBException so IntelliSense sees preprocessor
// literals rather than static constexpr member references — which trigger a false-positive
// "expression must be a modifiable lvalue" when writing to the arrays via snprintf.
// #undef'd after the class definition to avoid polluting other translation units.
#define CPP_DBC_DB_EXCEPTION_MARK_LEN 12
#define CPP_DBC_DB_EXCEPTION_MSG_CAP 64
// 12 (mark) + 2 (": ") + 64 (message) + 1 (null)
#define CPP_DBC_DB_EXCEPTION_FULL_MSG_MAX 79

namespace cpp_dbc
{

    /**
     * @brief Base exception class for all database-related errors
     *
     * Hybrid fixed/dynamic storage: a 79-byte fixed buffer holds the mark and
     * up to 64 characters of message with zero heap allocation. Messages longer
     * than 64 chars spill to a heap-allocated shared buffer; if the allocation
     * fails (out of memory), what() returns the truncated fixed buffer —
     * graceful degradation with no exception thrown.
     *
     * Layout of fixed buffer: "XXXXXXXXXXXX: <message>\0" — mark occupies the
     * first MARK_LEN bytes, followed by ": " and the message (truncated at
     * MSG_CAP chars). getMark() returns a string_view over the first MARK_LEN
     * bytes without any allocation.
     *
     * The call stack is heap-allocated on demand via shared_ptr — DBException
     * itself stores only a 16-byte pointer. When no callstack is passed the
     * pointer is nullptr and no heap memory is allocated at all.
     *
     * Object size breakdown (x86-64, best case: message ≤ 64 chars, no callstack):
     *
     *   std::exception vtable ptr ......  8 bytes
     *   m_full_message[79] ............. 79 bytes  (12 mark + 2 ": " + 64 msg + 1 null)
     *   m_overflow (shared_ptr, null) .. 16 bytes  (no heap alloc)
     *   m_callstack (shared_ptr, null) . 16 bytes  (no heap alloc)
     *   alignment padding ..............  1 byte   (compiler-dependent)
     *                                   ─────────
     *   Total ......................... ~120 bytes
     *
     * Worst case adds heap allocations for overflow (~fullSize bytes) and/or
     * CallStackCapture (~3,040 bytes), but the object itself stays ~120 bytes.
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
    class DBException final : public std::exception
    {
        // Fixed buffer layout: "XXXXXXXXXXXX: <message>\0"
        // - bytes [0,  MARK_LEN)         → mark, always exactly MARK_LEN chars (padded/truncated)
        // - bytes [MARK_LEN, MARK_LEN+2) → ": " separator, always present
        // - bytes after separator         → message (right-truncated at MSG_CAP chars)
        // Always populated, even when m_overflow exists — serves as fallback if heap alloc failed.
        // NOSONAR(cpp:S5945) — char[] required: allocation-free by design so the constructor
        // is noexcept; std::array<char,N> is layout-identical but forces .data() at every
        // C-API boundary (snprintf, what()) with zero benefit.
        char m_full_message[CPP_DBC_DB_EXCEPTION_FULL_MSG_MAX]{}; // NOSONAR(cpp:S5945)

        // Heap-allocated full (untruncated) message for messages exceeding MSG_CAP chars.
        // nullptr when: (a) message fits in fixed buffer, or (b) heap allocation failed.
        // Allocated via new(std::nothrow) — never throws, returns nullptr on failure.
        // shared_ptr (not unique_ptr) because DBException is frequently copied in
        // catch-and-propagate patterns (894 occurrences across 104 files); the overflow
        // buffer is immutable after construction, so sharing via ref-count is safe and
        // keeps copy/move = default.
        std::shared_ptr<char[]> m_overflow{};

        std::shared_ptr<system_utils::CallStackCapture> m_callstack{};

    public:
        /**
         * @brief Construct a new DBException
         *
         * The fixed buffer is always populated (truncated at MSG_CAP chars).
         * If the message exceeds MSG_CAP, a heap buffer is allocated via
         * new(std::nothrow) to store the full message. On allocation failure,
         * what() returns the truncated fixed buffer — no exception is thrown.
         *
         * @param mark A unique 12-character alphanumeric error code
         * @param message The human-readable error message
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
            // Build the fixed buffer as "XXXXXXXXXXXX: <message>\0".
            // %-12.12s pads short marks with spaces on the right and truncates long marks,
            // so bytes [0, 12) are always the mark and byte 12 is always ':'.
            // This fixed layout makes getMark() a trivial O(1) string_view slice with no
            // branching or searching.
            // Empty mark is replaced with "MARK_NOT_DEF" so the layout contract always holds.
            // NOSONAR(cpp:S6494) — noexcept + allocation-free; std::format can throw and heap-allocates
            const char *effectiveMark = mark.empty() ? "MARK_NOT_DEF" : mark.c_str();
            std::snprintf(m_full_message, sizeof(m_full_message),
                          "%-12.12s: %s", effectiveMark, message.c_str());

            // If the message exceeds the fixed capacity, attempt to allocate a heap
            // buffer for the full (untruncated) message. On allocation failure,
            // what() gracefully degrades to the truncated fixed buffer.
            if (message.size() > CPP_DBC_DB_EXCEPTION_MSG_CAP)
            {
                const std::size_t fullSize =
                    CPP_DBC_DB_EXCEPTION_MARK_LEN + 2 + message.size() + 1;
                // new(std::nothrow) returns nullptr on allocation failure — never throws.
                auto *buf = new(std::nothrow) char[fullSize]; // NOSONAR(cpp:S5945)
                if (buf)
                {
                    std::snprintf(buf, fullSize,
                                  "%-12.12s: %s", effectiveMark, message.c_str());
                    m_overflow.reset(buf, std::default_delete<char[]>{});
                }
            }
        }

        ~DBException() override = default;

        // Default copy and move are correct: char arrays are trivially copyable,
        // shared_ptr copy/move only adjust the reference count — no deep allocation.
        // m_overflow is shared_ptr (not unique_ptr) specifically to keep these = default.
        DBException(const DBException &) = default;
        DBException &operator=(const DBException &) = default;
        DBException(DBException &&) = default;
        DBException &operator=(DBException &&) = default;

        /**
         * @brief Get the full error message as a C-string
         *
         * Returns the untruncated message if heap allocation succeeded, or the
         * truncated fixed buffer otherwise. Format: "MARK: message".
         * Required by std::exception contract. Never allocates.
         */
        const char *what() const noexcept override
        {
            return m_overflow ? m_overflow.get() : m_full_message;
        }

        /**
         * @brief Get the full error message as a string_view
         *
         * Returns a view over the same buffer as what(). Never allocates.
         * The view is valid for the lifetime of this DBException object.
         *
         * ```cpp
         * catch (const cpp_dbc::DBException &e) {
         *     std::cerr << e.what_s() << std::endl;
         * }
         * ```
         */
        std::string_view what_s() const noexcept
        {
            return m_overflow
                ? std::string_view{m_overflow.get()}
                : std::string_view{m_full_message};
        }

        /**
         * @brief Get the unique error code identifying this error
         *
         * Returns a string_view over the first MARK_LEN bytes of the fixed buffer.
         * Always exactly MARK_LEN chars — short marks are space-padded, long marks truncated.
         * The mark is always in the fixed buffer, even when m_overflow exists.
         *
         * @return string_view of exactly MARK_LEN characters
         */
        std::string_view getMark() const noexcept
        {
            // The mark always occupies bytes [0, MARK_LEN) — guaranteed by the constructor
            // which uses %-12.12s to pad/truncate to exactly MARK_LEN chars.
            return std::string_view{m_full_message, CPP_DBC_DB_EXCEPTION_MARK_LEN};
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
            // Clamp count to MAX_FRAMES before building the span to guard against
            // a corrupted count field overrunning the fixed-size frames array.
            const std::size_t safeCount = std::min(m_callstack->count,
                                                   system_utils::CallStackCapture::MAX_FRAMES);
            return std::span<const system_utils::StackFrame>{m_callstack->frames, safeCount};
        }
    };

} // namespace cpp_dbc

#undef CPP_DBC_DB_EXCEPTION_MARK_LEN
#undef CPP_DBC_DB_EXCEPTION_MSG_CAP
#undef CPP_DBC_DB_EXCEPTION_FULL_MSG_MAX

#endif // CPP_DBC_CORE_DB_EXCEPTION_HPP
