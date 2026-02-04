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
 * @file db_expected.hpp
 * @brief Simplified expected<T, E> implementation for C++11+
 *
 * This is a minimal implementation of expected<T, E> compatible with C++11
 * and forward compatible with std::expected (C++23). It provides:
 * - Basic expected<T, E> functionality
 * - Support for void as T (expected<void, E>)
 * - noexcept guarantees where possible
 * - Compatible API with std::expected
 *
 * Note: This is NOT a complete implementation of std::expected. It provides
 * only the subset needed for cpp_dbc. If std::expected is available (C++23),
 * we use that instead via a type alias.
 */

#ifndef CPP_DBC_CORE_DB_EXPECTED_HPP
#define CPP_DBC_CORE_DB_EXPECTED_HPP

// Try to use std::expected if available (C++23)
#if __cplusplus >= 202302L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202302L)
#if __has_include(<expected>)
#define CPP_DBC_HAS_STD_EXPECTED 1 // NOSONAR - Macro needed for conditional compilation
#endif
#endif

#ifdef CPP_DBC_HAS_STD_EXPECTED
// Use native std::expected
#include <expected>

namespace cpp_dbc
{
    template <typename T, typename E>
    using expected = std::expected<T, E>;

    template <typename E>
    using unexpected = std::unexpected<E>;
}
#else
// Provide our own simplified implementation
#include <type_traits>
#include <utility>
#include <new>
#include <exception>

namespace cpp_dbc
{

    // Forward declarations
    template <typename T, typename E>
    class expected;
    template <typename E>
    class unexpected;

    /**
     * @brief Wrapper for error values used with expected<T, E>
     *
     * ```cpp
     * // Return an error from a nothrow function
     * return cpp_dbc::unexpected(cpp_dbc::DBException("ABC123DEF456", "Something failed"));
     * ```
     *
     * @tparam E The error type
     */
    template <typename E>
    class unexpected
    {
    private:
        E m_error;

    public:
        unexpected() = delete;

        constexpr explicit unexpected(const E &e) : m_error(e) {}
        constexpr explicit unexpected(E &&e) : m_error(std::move(e)) {}

        constexpr const E &error() const & noexcept { return m_error; }
        constexpr E &error() & noexcept { return m_error; }
        constexpr const E &&error() const && noexcept { return std::move(m_error); }
        constexpr E &&error() && noexcept { return std::move(m_error); }
    };

    /**
     * @brief A value-or-error type, compatible with std::expected (C++23)
     *
     * Used by all nothrow API methods in cpp_dbc. Every method that has a
     * throwing version also has a nothrow overload that takes std::nothrow
     * as first parameter and returns expected<T, DBException>.
     *
     * ```cpp
     * // Nothrow API pattern
     * auto result = conn->executeQuery(std::nothrow, "SELECT 1");
     * if (result.has_value()) {
     *     auto rs = result.value();
     *     // ... use result set ...
     * } else {
     *     std::cerr << result.error().what_s() << std::endl;
     * }
     * ```
     *
     * @tparam T The value type on success
     * @tparam E The error type on failure (typically DBException)
     */
    template <typename T, typename E>
    class expected
    {
    private:
        union
        {
            T m_value;
            E m_error;
        };
        bool m_has_value;

        void destroy() noexcept
        {
            if (m_has_value)
            {
                m_value.~T();
            }
            else
            {
                m_error.~E();
            }
        }

    public:
        using value_type = T;
        using error_type = E;

        // Constructors for value
        constexpr expected(const T &value)
            : m_value(value), m_has_value(true) {}

        constexpr expected(T &&value)
            : m_value(std::move(value)), m_has_value(true) {}

        // Constructor for error
        constexpr expected(const unexpected<E> &unex)
            : m_error(unex.error()), m_has_value(false) {}

        constexpr expected(unexpected<E> &&unex)
            : m_error(std::move(unex.error())), m_has_value(false) {}

        // Copy constructor
        expected(const expected &other) : m_has_value(other.m_has_value)
        {
            if (m_has_value)
            {
                new (&m_value) T(other.m_value);
            }
            else
            {
                new (&m_error) E(other.m_error);
            }
        }

        // Move constructor
        expected(expected &&other) noexcept : m_has_value(other.m_has_value)
        {
            if (m_has_value)
            {
                new (&m_value) T(std::move(other.m_value));
            }
            else
            {
                new (&m_error) E(std::move(other.m_error));
            }
        }

        // Destructor
        ~expected()
        {
            destroy();
        }

        // Copy assignment
        expected &operator=(const expected &other)
        {
            if (this != &other)
            {
                destroy();
                m_has_value = other.m_has_value;
                if (m_has_value)
                {
                    new (&m_value) T(other.m_value);
                }
                else
                {
                    new (&m_error) E(other.m_error);
                }
            }
            return *this;
        }

        // Move assignment
        expected &operator=(expected &&other) noexcept
        {
            if (this != &other)
            {
                destroy();
                m_has_value = other.m_has_value;
                if (m_has_value)
                {
                    new (&m_value) T(std::move(other.m_value));
                }
                else
                {
                    new (&m_error) E(std::move(other.m_error));
                }
            }
            return *this;
        }

        // Observers
        constexpr bool has_value() const noexcept { return m_has_value; }
        constexpr explicit operator bool() const noexcept { return m_has_value; }

        // Value access
        constexpr T &value() &
        {
            if (!m_has_value)
            {
                throw std::exception(); // In real code, throw bad_expected_access
            }
            return m_value;
        }

        constexpr const T &value() const &
        {
            if (!m_has_value)
            {
                throw std::exception();
            }
            return m_value;
        }

        constexpr T &&value() &&
        {
            if (!m_has_value)
            {
                throw std::exception();
            }
            return std::move(m_value);
        }

        constexpr const T &&value() const &&
        {
            if (!m_has_value)
            {
                throw std::exception();
            }
            return std::move(m_value);
        }

        // Error access
        constexpr E &error() & noexcept { return m_error; }
        constexpr const E &error() const & noexcept { return m_error; }
        constexpr E &&error() && noexcept { return std::move(m_error); }
        constexpr const E &&error() const && noexcept { return std::move(m_error); }

        // Pointer-like access (unchecked)
        constexpr T *operator->() noexcept { return &m_value; }
        constexpr const T *operator->() const noexcept { return &m_value; }
        constexpr T &operator*() & noexcept { return m_value; }
        constexpr const T &operator*() const & noexcept { return m_value; }
        constexpr T &&operator*() && noexcept { return std::move(m_value); }
        constexpr const T &&operator*() const && noexcept { return std::move(m_value); }

        // value_or
        template <typename U>
        constexpr T value_or(U &&default_value) const &
        {
            return m_has_value ? m_value : static_cast<T>(std::forward<U>(default_value));
        }

        template <typename U>
        constexpr T value_or(U &&default_value) &&
        {
            return m_has_value ? std::move(m_value) : static_cast<T>(std::forward<U>(default_value));
        }
    };

    /**
     * @brief Specialization of expected for void
     */
    template <typename E>
    class expected<void, E>
    {
    private:
        union
        {
            char m_dummy; // For trivial default constructor
            E m_error;
        };
        bool m_has_value;

    public:
        using value_type = void;
        using error_type = E;

        // Default constructor (success)
        constexpr expected() noexcept : m_dummy(), m_has_value(true) {}

        // Constructor for error
        constexpr expected(const unexpected<E> &unex)
            : m_error(unex.error()), m_has_value(false) {}

        constexpr expected(unexpected<E> &&unex)
            : m_error(std::move(unex.error())), m_has_value(false) {}

        // Copy constructor
        expected(const expected &other) : m_has_value(other.m_has_value)
        {
            if (!m_has_value)
            {
                new (&m_error) E(other.m_error);
            }
        }

        // Move constructor
        expected(expected &&other) noexcept : m_has_value(other.m_has_value)
        {
            if (!m_has_value)
            {
                new (&m_error) E(std::move(other.m_error));
            }
        }

        // Destructor
        ~expected()
        {
            if (!m_has_value)
            {
                m_error.~E();
            }
        }

        // Copy assignment
        expected &operator=(const expected &other)
        {
            if (this != &other)
            {
                if (!m_has_value)
                {
                    m_error.~E();
                }
                m_has_value = other.m_has_value;
                if (!m_has_value)
                {
                    new (&m_error) E(other.m_error);
                }
            }
            return *this;
        }

        // Move assignment
        expected &operator=(expected &&other) noexcept
        {
            if (this != &other)
            {
                if (!m_has_value)
                {
                    m_error.~E();
                }
                m_has_value = other.m_has_value;
                if (!m_has_value)
                {
                    new (&m_error) E(std::move(other.m_error));
                }
            }
            return *this;
        }

        // Observers
        constexpr bool has_value() const noexcept { return m_has_value; }
        constexpr explicit operator bool() const noexcept { return m_has_value; }

        // Value access (void - just check)
        void value() const
        {
            if (!m_has_value)
            {
                throw std::exception();
            }
        }

        // Error access
        constexpr E &error() & noexcept { return m_error; }
        constexpr const E &error() const & noexcept { return m_error; }
        constexpr E &&error() && noexcept { return std::move(m_error); }
        constexpr const E &&error() const && noexcept { return std::move(m_error); }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_HAS_STD_EXPECTED

#endif // CPP_DBC_CORE_DB_EXPECTED_HPP
