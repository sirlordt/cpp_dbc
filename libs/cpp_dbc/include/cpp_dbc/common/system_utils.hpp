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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file system_utils.hpp
 @brief Implementation for the cpp_dbc library

*/

#ifndef SYSTEM_UTILS_HPP
#define SYSTEM_UTILS_HPP

#include <charconv>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <iostream>
#include <chrono>
#include <ctime>
#include <string>
#include <string_view>

// Platform-specific includes for write() syscall (eliminates Helgrind false positives)
#ifdef _WIN32
#include <Windows.h> // GetCurrentThreadId() used in getThreadId()
#include <io.h>      // _write(), _fileno()
#else
#include <unistd.h> // write(), STDOUT_FILENO
#endif

namespace cpp_dbc::system_utils
{

    // Declare the mutex as external
    extern std::recursive_mutex global_cout_mutex; // NOSONAR - Mutex cannot be const as it needs to be locked/unlocked

// #define used for array dimensions so IntelliSense sees a preprocessor literal rather
// than a static constexpr member reference — which triggers a false-positive
// "expression must be a modifiable lvalue" when writing to the arrays via strncpy.
// The static constexpr members below remain the canonical, type-safe API for callers.
// Each macro is #undef'd immediately after its struct to avoid namespace pollution.
#define CPP_DBC_STACKFRAME_FILE_MAX     150
#define CPP_DBC_STACKFRAME_FUNCTION_MAX 150

    /**
     * @brief Represents a single frame in a captured call stack
     *
     * Fixed-size, allocation-free struct. All string fields are char arrays with
     * defined maximum lengths. Values that exceed the limit are truncated on the
     * LEFT side (keeping the suffix), preserving the method name and parameters
     * which are more useful for debugging than the namespace/return type prefix.
     * Used by DBException and CallStackCapture to store stack trace information.
     */
    struct StackFrame
    {
        static constexpr std::size_t FILE_MAX     = CPP_DBC_STACKFRAME_FILE_MAX;     ///< Max bytes for source file path (incl. null); left-truncated if longer
        static constexpr std::size_t FUNCTION_MAX = CPP_DBC_STACKFRAME_FUNCTION_MAX; ///< Max bytes for function name (incl. null); left-truncated if longer

        char file[CPP_DBC_STACKFRAME_FILE_MAX]{};
        char function[CPP_DBC_STACKFRAME_FUNCTION_MAX]{};
        int  line{0};
    };

#undef CPP_DBC_STACKFRAME_FILE_MAX
#undef CPP_DBC_STACKFRAME_FUNCTION_MAX

#define CPP_DBC_CALLSTACK_MAX_FRAMES 10

    /**
     * @brief Fixed-size container for up to 10 captured stack frames
     *
     * Returned as a heap-allocated shared_ptr by captureCallStack(), so DBException
     * stores only a pointer (16 bytes) instead of the full struct inline.
     * The frames themselves are fixed-size char arrays — no further allocation occurs.
     */
    struct CallStackCapture
    {
        static constexpr std::size_t MAX_FRAMES = CPP_DBC_CALLSTACK_MAX_FRAMES; ///< Maximum number of captured frames

        StackFrame  frames[CPP_DBC_CALLSTACK_MAX_FRAMES]{};
        std::size_t count{0};
    };

#undef CPP_DBC_CALLSTACK_MAX_FRAMES

    /**
     * @brief Result structure for parsing database connection URLs
     *
     * Used by parseDBURL to return the parsed components of a database URL.
     * Supports URLs in the format: scheme://host:port/database
     * Also supports IPv6 addresses: scheme://[::1]:port/database
     * And local connections (no host): scheme:///path/to/database
     */
    struct ParsedDBURL
    {
        std::string host;     ///< Host name or IP address (without brackets for IPv6)
        int port;             ///< Port number
        std::string database; ///< Database name or path
        bool isLocal;         ///< True if this is a local connection (no host specified)
    };

    /**
     * @brief RAII guard for atomic flags
     *
     * Sets an atomic<T> to a specified value on construction and restores it to
     * another value on destruction. Useful for preventing re-entrancy or signaling
     * operation state across threads. Exception-safe through RAII guarantees.
     *
     * Example usage:
     * ```cpp
     * // Boolean flag
     * std::atomic<bool> m_processing{false};
     * {
     *     cpp_dbc::system_utils::AtomicGuard<bool> guard(m_processing, true, false);
     *     // m_processing is now true
     *     // ... do work ...
     * } // m_processing automatically reset to false
     *
     * // Integer counter
     * std::atomic<int> m_refCount{0};
     * {
     *     cpp_dbc::system_utils::AtomicGuard<int> guard(m_refCount, 1, 0);
     *     // m_refCount is now 1
     *     // ... do work ...
     * } // m_refCount reset to 0
     *
     * // Enum state
     * enum class State { Idle, Processing };
     * std::atomic<State> m_state{State::Idle};
     * {
     *     cpp_dbc::system_utils::AtomicGuard<State> guard(m_state, State::Processing, State::Idle);
     *     // m_state is now Processing
     *     // ... do work ...
     * } // m_state reset to Idle
     * ```
     *
     * @tparam T Type of the atomic value (bool, int, enum, etc.)
     * @note Non-copyable and non-movable to prevent accidental double-reset
     */
    template <typename T>
    class AtomicGuard
    {
    private:
        std::atomic<T> &m_flag;
        T m_resetValue;

    public:
        /**
         * @brief Constructs the guard and sets the flag to setValue
         * @param flag Reference to the atomic<T> to guard
         * @param setValue Value to set on construction
         * @param resetValue Value to restore on destruction
         */
        explicit AtomicGuard(std::atomic<T> &flag, T setValue, T resetValue) noexcept
            : m_flag(flag), m_resetValue(resetValue)
        {
            m_flag.store(setValue, std::memory_order_release);
        }

        /**
         * @brief Destroys the guard and resets the flag to resetValue
         */
        ~AtomicGuard() noexcept
        {
            m_flag.store(m_resetValue, std::memory_order_release);
        }

        // Prevent copying and moving to avoid double-reset bugs
        AtomicGuard(const AtomicGuard &) = delete;
        AtomicGuard &operator=(const AtomicGuard &) = delete;
        AtomicGuard(AtomicGuard &&) = delete;
        AtomicGuard &operator=(AtomicGuard &&) = delete;
    };

    /**
     * @brief Parse a database connection URL into its components
     *
     * Parses URLs in the format: prefix://host:port/database
     * Supports:
     * - IPv4 addresses and hostnames: prefix://localhost:3306/mydb
     * - IPv6 addresses with brackets: prefix://[::1]:3306/mydb
     * - Default ports: prefix://localhost/mydb (uses defaultPort)
     * - Local connections: prefix:///path/to/db (when allowLocalConnection is true)
     * - URLs without database: prefix://localhost:3306 (when requireDatabase is false)
     *
     * ```cpp
     * cpp_dbc::system_utils::ParsedDBURL result;
     * if (cpp_dbc::system_utils::parseDBURL("cpp_dbc:mysql://[::1]:3306/testdb",
     *                                        "cpp_dbc:mysql://", 3306, result)) {
     *     // result.host = "::1", result.port = 3306, result.database = "testdb"
     * }
     * ```
     *
     * @param url The full URL to parse
     * @param expectedPrefix The expected URL prefix (e.g., "cpp_dbc:mysql://")
     * @param defaultPort Default port to use if not specified in the URL
     * @param result Output structure containing the parsed components
     * @param allowLocalConnection If true, allows URLs without host (e.g., prefix:///path)
     * @param requireDatabase If true, the URL must include a database/path component
     * @return true if parsing succeeded, false otherwise
     */
    bool parseDBURL(std::string_view url,
                    std::string_view expectedPrefix,
                    int defaultPort,
                    ParsedDBURL &result,
                    bool allowLocalConnection = false,
                    bool requireDatabase = true) noexcept;

    /**
     * @brief Thread-safe print function using a global mutex
     *
     * ```cpp
     * cpp_dbc::system_utils::safePrint("DB", "Connected to MySQL");
     * // Output: DB: Connected to MySQL
     * ```
     *
     * @param mark Prefix label for the message
     * @param message The message to print
     */
    inline void safePrint(const std::string &mark, const std::string &message)
    {
        std::scoped_lock lock(global_cout_mutex);

        // CRITICAL: Build output string BEFORE writing to avoid Helgrind false positives
        // Using write() syscall instead of std::cout eliminates data races on FILE* buffer
        // because write() is atomic and doesn't use buffered I/O that Helgrind can't track
        std::string output = mark + ": " + message + "\n";

#ifdef _WIN32
        // Windows: use _write() syscall
        _write(_fileno(stdout), output.c_str(), static_cast<unsigned int>(output.size()));
#else
        // POSIX: use write() syscall
        // Ignore return value - this is debug output, not critical
        [[maybe_unused]] auto result = write(STDOUT_FILENO, output.c_str(), output.size());
#endif
    }

    /** @brief Get current time as "HH:MM:SS.mmm" string */
    inline std::string currentTimeMillis()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        std::time_t now_c = system_clock::to_time_t(now);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &now_c);
#else
        localtime_r(&now_c, &tm);
#endif

        // Stack buffer: "HH:MM:SS.mmm" = 12 chars + null
        // std::format / std::chrono::format require GCC 13+; this project targets GCC 11.
        // The stack buffer + strftime + snprintf pattern is correct, zero-allocation, and
        // thread-safe via localtime_r/localtime_s.
        char buf[16]; // NOSONAR(cpp:S5945) — see comment above
        std::size_t n = std::strftime(buf, sizeof(buf), "%H:%M:%S.", &tm); // NOSONAR(cpp:S6229) — see comment above
        std::snprintf(buf + n, sizeof(buf) - n, "%03d", static_cast<int>(ms.count())); // NOSONAR(cpp:S6494) — see comment above
        return buf;
    }

    /** @brief Get current time as "HH:MM:SS.microseconds" string (high precision) */
    inline std::string currentTimeMicros()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto micros = duration_cast<microseconds>(now.time_since_epoch()).count();
        std::time_t now_c = micros / 1000000;
        auto usecs = micros % 1000000;

        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &now_c);
#else
        localtime_r(&now_c, &tm);
#endif

        // Stack buffer: "HH:MM:SS.uuuuuu" = 15 chars + null
        // std::format / std::chrono::format require GCC 13+; this project targets GCC 11.
        // The stack buffer + strftime + snprintf pattern is correct, zero-allocation, and
        // thread-safe via localtime_r/localtime_s.
        char buf[20]; // NOSONAR(cpp:S5945) — see comment above
        std::size_t n = std::strftime(buf, sizeof(buf), "%H:%M:%S.", &tm); // NOSONAR(cpp:S6229) — see comment above
        std::snprintf(buf + n, sizeof(buf) - n, "%06ld", usecs); // NOSONAR(cpp:S6494) — see comment above; %06ld matches the actual type (long int)
        return buf;
    }

    /** @brief Get current timestamp as "YYYY-MM-DD HH:MM:SS.mmm" string */
    inline std::string getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &now_c);
#else
        localtime_r(&now_c, &tm);
#endif

        // Stack buffer: "YYYY-MM-DD HH:MM:SS.mmm" = 23 chars + null
        // std::format / std::chrono::format require GCC 13+; this project targets GCC 11.
        // The stack buffer + strftime + snprintf pattern is correct, zero-allocation, and
        // thread-safe via localtime_r/localtime_s.
        char buf[28]; // NOSONAR(cpp:S5945) — see comment above
        std::size_t n = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S.", &tm); // NOSONAR(cpp:S6229) — see comment above
        std::snprintf(buf + n, sizeof(buf) - n, "%03d", static_cast<int>(ms.count())); // NOSONAR(cpp:S6494) — see comment above
        return buf;
    }

    /** @brief Convert a thread ID to a short string (last 6 digits of hash, for non-current threads) */
    inline std::string threadIdToString(std::thread::id threadId) noexcept
    {
        // char array required by std::to_chars API; this function is noexcept so std::string
        // construction must be deferred until after to_chars writes into the buffer.
        char buf[12]; // NOSONAR(cpp:S5945) — see comment above
        // Truncate to last 6 digits: avoids the huge pthread_t value while staying useful for logs
        auto hash = std::hash<std::thread::id>{}(threadId) % 1000000UL;
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), hash);
        return std::string(buf, ptr);
    }

    /** @brief Get the current thread OS-native TID as a string (matches htop/ps output) */
    inline std::string getThreadId() noexcept
    {
        // char array required by std::to_chars API; this function is noexcept so std::string
        // construction must be deferred until after to_chars writes into the buffer.
        char buf[12]; // NOSONAR(cpp:S5945) — see comment above
#if defined(__linux__)
        // gettid() returns the kernel TID: short integer, same as shown in htop/ps
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), static_cast<unsigned long>(::gettid()));
#elif defined(_WIN32)
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), static_cast<unsigned long>(GetCurrentThreadId()));
#else
        // Fallback: truncated hash
        auto hash = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 1000000UL;
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), hash);
#endif
        return std::string(buf, ptr);
    }

    /** @brief Log a message with timestamp prefix (thread-safe) */
    inline void logWithTimestamp(std::string_view prefix, std::string_view message)
    {
        std::scoped_lock lock(global_cout_mutex);

        // Build output string before writing (same as safePrint)
        // reserve() + append() avoids intermediate allocations from operator+
        auto ts = getCurrentTimestamp();
        auto tid = getThreadId();
        std::string output;
        output.reserve(ts.size() + tid.size() + prefix.size() + message.size() + 12);
        output += '[';
        output.append(ts);
        output += "] [";
        output.append(tid);
        output += "] [";
        output.append(prefix);
        output += "] ";
        output.append(message);
        output += '\n';

#ifdef _WIN32
        _write(_fileno(stdout), output.c_str(), static_cast<unsigned int>(output.size()));
#else
        [[maybe_unused]] auto result = write(STDOUT_FILENO, output.c_str(), output.size());
#endif
    }

    /** @brief Log a message with timestamp prefix (thread-safe) */
    inline void logWithTimesMillis(std::string_view prefix, std::string_view message)
    {
        std::scoped_lock lock(global_cout_mutex);

        // Build output string before writing (same as safePrint)
        // reserve() + append() avoids intermediate allocations from operator+
        auto ts = currentTimeMillis();
        auto tid = getThreadId();
        std::string output;
        output.reserve(ts.size() + tid.size() + prefix.size() + message.size() + 12);
        output += '[';
        output.append(ts);
        output += "] [";
        output.append(tid);
        output += "] [";
        output.append(prefix);
        output += "] ";
        output.append(message);
        output += '\n';

#ifdef _WIN32
        _write(_fileno(stdout), output.c_str(), static_cast<unsigned int>(output.size()));
#else
        [[maybe_unused]] auto result = write(STDOUT_FILENO, output.c_str(), output.size());
#endif
    }

    /** @brief Log an INFO message with timestamp */
    inline void logWithTimestampInfo(const std::string &message)
    {
        logWithTimestamp("[INFO]", message);
    }

    /** @brief Log an INFO message with timestamp and mark */
    inline void logWithTimestampInfoMark(const std::string &mark, const std::string &message)
    {
        logWithTimestamp("[INFO] [" + mark + "]", message);
    }

    /** @brief Log a DEBUG message with timestamp */
    inline void logWithTimestampDebug(const std::string &message)
    {
        logWithTimestamp("[DEBUG]", message);
    }

    /** @brief Log a DEBUG message with timestamp and mark */
    inline void logWithTimestampDebugMark(const std::string &mark, const std::string &message)
    {
        logWithTimestamp("[DEBUG] [" + mark + "]", message);
    }

    /** @brief Log a WARNING message with timestamp */
    inline void logWithTimestampWarning(const std::string &message)
    {
        logWithTimestamp("[WARNING]", message);
    }

    /** @brief Log a WARNING message with timestamp and mark */
    inline void logWithTimestampWarningMark(const std::string &mark, const std::string &message)
    {
        logWithTimestamp("[WARNING] [" + mark + "]", message);
    }

    /** @brief Log an ERROR message with timestamp */
    inline void logWithTimestampError(const std::string &message)
    {
        logWithTimestamp("[ERROR]", message);
    }

    /** @brief Log an ERROR message with timestamp and mark */
    inline void logWithTimestampErrorMark(const std::string &mark, const std::string &message)
    {
        logWithTimestamp("[ERROR] [" + mark + "]", message);
    }

    /** @brief Log an EXCEPTION message with timestamp */
    inline void logWithTimestampException(const std::string &message)
    {
        logWithTimestamp("[EXCEPTION]", message);
    }

    /** @brief Log an EXCEPTION message with timestamp and mark */
    inline void logWithTimestampExceptionMark(const std::string &mark, const std::string &message)
    {
        logWithTimestamp("[EXCEPTION] [" + mark + "]", message);
    }

    /**
     * @brief Capture the current call stack for debugging
     *
     * Returns a heap-allocated shared_ptr<CallStackCapture>. The allocation is
     * wrapped in try/catch so the function remains noexcept — on allocation failure
     * it returns nullptr (no stack trace, but no crash or exception either).
     *
     * Storing the result in DBException costs only 16 bytes (the shared_ptr) instead
     * of the full CallStackCapture struct inline. When no callstack is passed to
     * DBException the pointer is nullptr and no heap memory is allocated at all.
     *
     * ```cpp
     * auto capture = cpp_dbc::system_utils::captureCallStack();
     * if (capture) { cpp_dbc::system_utils::printCallStack(*capture); }
     * ```
     *
     * @param captureAll If true, captures all frames; otherwise skips internal frames
     * @param skip Number of frames to skip from the top of the stack
     * @return shared_ptr to a CallStackCapture with up to MAX_FRAMES frames, or nullptr on failure
     */
    std::shared_ptr<CallStackCapture> captureCallStack(bool captureAll = false, int skip = 1) noexcept;

    /**
     * @brief Print a captured call stack to stdout
     * @param capture The CallStackCapture to print (no-op if nullptr)
     */
    void printCallStack(const std::shared_ptr<CallStackCapture> &capture);

    /**
     * @brief Print a span of stack frames to stdout
     * @param frames Span of StackFrame objects to print
     */
    void printCallStack(std::span<const StackFrame> frames);

    /**
     * @brief Get the full path to the current executable including its name
     *
     * Uses platform-specific methods:
     * - Linux: reads /proc/self/exe
     * - Windows: uses GetModuleFileName
     * - macOS: uses _NSGetExecutablePath
     *
     * @return Full path to executable, or empty string on failure
     */
    std::string getExecutablePathAndName();

    /**
     * @brief Get the directory containing the current executable
     *
     * @return Directory path with trailing separator, or "./" on failure
     */
    std::string getExecutablePath();

    /**
     * @brief Convert a snake_case string to lowerCamelCase
     *
     * Converts strings in snake_case format to lowerCamelCase:
     * - The first word remains lowercase
     * - Each subsequent word (after underscore) has its first letter capitalized
     * - All underscores are removed
     *
     * This function was originally created for MongoDB driver option name conversion,
     * where YAML configuration files use snake_case naming (following YAML conventions)
     * but the MongoDB C driver (mongoc) expects camelCase option names in connection URIs.
     *
     * **Common MongoDB option conversions:**
     * - `auth_source` → `authSource`
     * - `direct_connection` → `directConnection`
     * - `connect_timeout_ms` → `connectTimeoutMs`
     * - `server_selection_timeout_ms` → `serverSelectionTimeoutMs`
     *
     * **Note for MongoDB users:**
     * Timeout options should use the `_ms` suffix in YAML (e.g., `connect_timeout_ms: 5000`)
     * to ensure they convert correctly to mongoc's expected format (e.g., `connectTimeoutMs=5000`).
     *
     * **Usage examples:**
     * ```cpp
     * // MongoDB option conversion
     * auto authSource = cpp_dbc::system_utils::snakeCaseToLowerCamelCase("auth_source");
     * // authSource == "authSource"
     *
     * auto timeout = cpp_dbc::system_utils::snakeCaseToLowerCamelCase("connect_timeout_ms");
     * // timeout == "connectTimeoutMs"
     *
     * // General string conversion
     * auto myVar = cpp_dbc::system_utils::snakeCaseToLowerCamelCase("my_variable_name");
     * // myVar == "myVariableName"
     * ```
     *
     * @param snakeCase The snake_case string to convert
     * @return The converted lowerCamelCase string
     *
     * @note This is a generic utility function that follows standard naming convention rules.
     *       It does not hardcode specific option names, making it reusable across the entire
     *       codebase (tests, examples, drivers) while respecting the Open/Closed Principle.
     */
    std::string snakeCaseToLowerCamelCase(const std::string &snakeCase);

} // namespace cpp_dbc::system_utils

#endif // SYSTEM_UTILS_HPP
