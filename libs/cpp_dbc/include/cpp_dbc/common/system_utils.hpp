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

#include <mutex>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cpp_dbc::system_utils
{

        // Declare the mutex as external
        extern std::mutex global_cout_mutex; // NOSONAR - Mutex cannot be const as it needs to be locked/unlocked

        /**
         * @brief Represents a single frame in a captured call stack
         *
         * Used by DBException to store stack trace information for debugging.
         */
        struct StackFrame
        {
            std::string file;
            int line;
            std::string function;
        };

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
            std::string host;      ///< Host name or IP address (without brackets for IPv6)
            int port;              ///< Port number
            std::string database;  ///< Database name or path
            bool isLocal;          ///< True if this is a local connection (no host specified)
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
            std::cout << mark << ": " << message << std::endl;
        }

        /** @brief Get current time as "HH:MM:SS.mmm" string */
        inline std::string currentTimeMillis()
        {
            using namespace std::chrono;

            // Get current system time
            auto now = system_clock::now();

            // Convert to time_t for formatting hours/minutes/seconds
            std::time_t now_c = system_clock::to_time_t(now);
            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &now_c);
#else
            localtime_r(&now_c, &tm);
#endif

            // Extract milliseconds
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

            // Format to string
            std::ostringstream oss;
            oss << std::put_time(&tm, "%H:%M:%S")
                << '.' << std::setw(3) << std::setfill('0') << ms.count();

            return oss.str();
        }

        /** @brief Get current timestamp as "[YYYY-MM-DD HH:MM:SS.mmm]" string */
        inline std::string getCurrentTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);

            // Get milliseconds
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()) %
                      1000;

            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &time_t_now);
#else
            localtime_r(&time_t_now, &tm);
#endif

            std::stringstream ss;
            ss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S");
            ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";

            return ss.str();
        }

        /** @brief Log a message with timestamp prefix (thread-safe) */
        inline void logWithTimestamp(const std::string &prefix, const std::string &message)
        {
            std::scoped_lock lock(global_cout_mutex);
            std::cout << getCurrentTimestamp() << " " << prefix << " " << message << std::endl;
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
         * ```cpp
         * auto frames = cpp_dbc::system_utils::captureCallStack();
         * cpp_dbc::system_utils::printCallStack(frames);
         * ```
         *
         * @param captureAll If true, captures all frames; otherwise skips internal frames
         * @param skip Number of frames to skip from the top of the stack
         * @return Vector of StackFrame objects representing the call stack
         */
        std::vector<StackFrame> captureCallStack(bool captureAll = false, int skip = 1) noexcept;

        /**
         * @brief Print a captured call stack to stdout
         * @param frames The stack frames to print
         */
        void printCallStack(const std::vector<StackFrame> &frames);
} // namespace cpp_dbc::system_utils

#endif // SYSTEM_UTILS_HPP
