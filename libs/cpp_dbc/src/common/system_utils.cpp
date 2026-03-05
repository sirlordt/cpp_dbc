/*

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

 @file system_utils.cpp
 @brief Implementation for the cpp_dbc library

*/

#include "cpp_dbc/common/system_utils.hpp"
#include "cpp_dbc/backward.hpp"

#include <vector>
#include <filesystem>
#include <thread>

#ifdef __linux__
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace cpp_dbc::system_utils
{

    // Define the global mutex
    std::recursive_mutex global_cout_mutex{}; // NOSONAR - Mutex cannot be const as it needs to be locked/unlocked

    bool shouldSkipFrame(std::string_view filename, std::string_view function)
    {

        // Skip backward-cpp internals
        if (filename.contains("backward.hpp"))
        {
            return true;
        }

        // Skip libc/system frames
        if (filename.contains("libc") ||
            filename.contains("csu/") ||
            filename.contains("sysdeps/"))
        {
            return true;
        }

        // Skip system functions
        if (function.contains("__libc") ||
            function.starts_with("_start") ||
            function == "??")
        {
            return true;
        }

        // Skip unresolved frames (??:0)
        if (filename == "??" || filename.empty())
        {
            return true;
        }

        // Skip our own tracing infrastructure
        if (function.contains("captureCallStack") ||
            function.contains("printCallStack"))
        {
            return true;
        }

        return false;
    }

    std::shared_ptr<CallStackCapture> captureCallStack([[maybe_unused]] bool captureAll, [[maybe_unused]] int skip) noexcept
    {
#if BACKWARD_HAS_DW == 0
        // When libdw support is disabled (--dw-off flag), skip stack trace capture
        // to eliminate overhead in production builds. Return nullptr immediately.
        return nullptr;
#else
        try
        {
            // CRITICAL: backward-cpp (TraceResolver, StackTrace) has internal shared state
            // that is NOT thread-safe. Without this mutex, concurrent calls from multiple
            // threads cause data races detected by Helgrind/ThreadSanitizer.
            // This recursive_mutex serializes all stack trace capture operations globally.
            static std::recursive_mutex stackTraceMutex;
            std::scoped_lock lock(stackTraceMutex);

            backward::StackTrace st;
            st.load_here(32);
            backward::TraceResolver tr;
            tr.load_stacktrace(st);

            // Allocate the capture on the heap. The try/catch ensures bad_alloc cannot
            // escape — returning nullptr is a safe degradation (no stack trace, no crash).
            auto capture = std::make_shared<CallStackCapture>();

            const auto startFrame = static_cast<std::size_t>(skip);
            for (std::size_t i = startFrame; i < st.size() && capture->count < CallStackCapture::MAX_FRAMES; ++i)
            {
                backward::ResolvedTrace trace = tr.resolve(st[i]);

                // backward-cpp resolves to std::string internally; we keep those as
                // temporaries and copy only the result into the fixed char buffers.
                const std::string &filename = trace.source.filename;
                const std::string &function = trace.object_function;

                if (!captureAll && shouldSkipFrame(filename, function))
                {
                    continue;
                }

                auto &frame = capture->frames[capture->count];

                // Copy source file path — left-truncated if path exceeds FILE_MAX-1 chars,
                // preserving the filename/suffix which is more useful than the directory prefix.
                {
                    const std::string &src = filename.empty() ? std::string{"??"} : filename;
                    constexpr std::size_t maxLen = StackFrame::FILE_MAX - 1;
                    if (src.size() > maxLen)
                    {
                        // Keep the rightmost maxLen characters (filename + nearest dirs)
                        std::strncpy(frame.file, src.c_str() + (src.size() - maxLen), maxLen);
                    }
                    else
                    {
                        std::strncpy(frame.file, src.c_str(), maxLen);
                    }
                    frame.file[maxLen] = '\0';
                }

                // Copy demangled function name — left-truncated if name exceeds FUNCTION_MAX-1 chars,
                // preserving the method name and parameters rather than the namespace/return type prefix.
                {
                    const std::string &src = function.empty() ? std::string{"??"} : function;
                    constexpr std::size_t maxLen = StackFrame::FUNCTION_MAX - 1;
                    if (src.size() > maxLen)
                    {
                        // Keep the rightmost maxLen characters (method name + parameters)
                        std::strncpy(frame.function, src.c_str() + (src.size() - maxLen), maxLen);
                    }
                    else
                    {
                        std::strncpy(frame.function, src.c_str(), maxLen);
                    }
                    frame.function[maxLen] = '\0';
                }

                frame.line = trace.source.line;
                ++capture->count;
            }

            return capture;
        }
        catch (...)
        {
            // Return nullptr if stack trace or allocation fails — noexcept guarantee.
            // The caller (DBException) treats nullptr as "no stack trace available".
            return nullptr;
        }
#endif
    }

    void printCallStack(const std::shared_ptr<CallStackCapture> &capture)
    {
        if (!capture)
        {
            std::cout << "Stack trace: (not available)\n";
            return;
        }
        std::cout << "Stack trace (" << capture->count << " frames):\n";
        for (std::size_t i = 0; i < capture->count; ++i)
        {
            std::cout << "  #" << i << " "
                      << capture->frames[i].file << ":" << capture->frames[i].line
                      << " in " << capture->frames[i].function << "\n";
        }
    }

    void printCallStack(std::span<const StackFrame> frames)
    {
        std::cout << "Stack trace (" << frames.size() << " frames):\n";
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            std::cout << "  #" << i << " "
                      << frames[i].file << ":" << frames[i].line
                      << " in " << frames[i].function << "\n";
        }
    }

    bool parseDBURL(std::string_view url,
                    std::string_view expectedPrefix,
                    int defaultPort,
                    ParsedDBURL &result,
                    bool allowLocalConnection,
                    bool requireDatabase) noexcept
    {
        try
        {
            // Initialize result
            result.host = "";
            result.port = defaultPort;
            result.database = "";
            result.isLocal = false;

            // Check prefix
            if (!url.starts_with(expectedPrefix))
            {
                return false;
            }

            // Extract the part after the prefix
            std::string rest{url.substr(expectedPrefix.length())};

            // Check for empty rest
            if (rest.empty())
            {
                return !requireDatabase;
            }

            // Check for local connection (starts with '/')
            // For non-local drivers, '/' indicates start of database path handled in normal parsing below
            if (rest[0] == '/' && allowLocalConnection)
            {
                // Local connection: prefix:///path/to/database
                result.isLocal = true;
                result.host = "";
                result.database = rest;
                return !requireDatabase || !result.database.empty();
            }

            // Check for IPv6 address (starts with '[')
            if (!rest.empty() && rest[0] == '[')
            {
                // Find closing bracket for IPv6 address
                size_t bracketEnd = rest.find(']');
                if (bracketEnd == std::string::npos)
                {
                    return false; // Invalid IPv6: missing closing bracket
                }

                // Extract IPv6 host (without brackets)
                result.host = rest.substr(1, bracketEnd - 1);

                // Move past the closing bracket
                rest = rest.substr(bracketEnd + 1);

                // Check what comes after the IPv6 address
                if (rest.empty())
                {
                    // Just IPv6 address, no port or database
                    return !requireDatabase;
                }

                if (rest[0] == ':')
                {
                    // Port specified after IPv6
                    size_t slashPos = rest.find('/');
                    if (slashPos == std::string::npos)
                    {
                        // No database, just port
                        try
                        {
                            result.port = std::stoi(rest.substr(1));
                        }
                        catch (...)
                        {
                            return false; // Invalid port
                        }
                        return !requireDatabase;
                    }

                    // Port and database
                    try
                    {
                        result.port = std::stoi(rest.substr(1, slashPos - 1));
                    }
                    catch (...)
                    {
                        return false; // Invalid port
                    }
                    result.database = rest.substr(slashPos + 1);
                    return !requireDatabase || !result.database.empty();
                }

                if (rest[0] == '/')
                {
                    // No port, just database
                    result.database = rest.substr(1);
                    return !requireDatabase || !result.database.empty();
                }

                // Invalid character after IPv6 address
                return false;
            }

            // Standard IPv4/hostname parsing
            size_t colonPos = rest.find(':');
            size_t slashPos = rest.find('/');

            if (slashPos == std::string::npos)
            {
                // No database path
                if (colonPos != std::string::npos)
                {
                    // host:port (no database)
                    result.host = rest.substr(0, colonPos);
                    try
                    {
                        result.port = std::stoi(rest.substr(colonPos + 1));
                    }
                    catch (...)
                    {
                        return false; // Invalid port
                    }
                }
                else
                {
                    // Just host (no port, no database)
                    result.host = rest;
                }
                return !requireDatabase;
            }

            // Has database path
            if (colonPos != std::string::npos && colonPos < slashPos)
            {
                // host:port/database
                result.host = rest.substr(0, colonPos);
                try
                {
                    result.port = std::stoi(rest.substr(colonPos + 1, slashPos - colonPos - 1));
                }
                catch (...)
                {
                    return false; // Invalid port
                }
            }
            else
            {
                // host/database (no port)
                result.host = rest.substr(0, slashPos);
            }

            result.database = rest.substr(slashPos + 1);

            // For Firebird-style paths, include the leading slash in database
            // This is handled by the caller if needed

            return !requireDatabase || !result.database.empty();
        }
        catch (...)
        {
            // Catch any unexpected exceptions - noexcept guarantee
            return false;
        }
    }

    // getCurrentTimestamp and logWithTimestamp are already defined as inline in the header file
    // No need to redefine them here

    std::string getExecutablePathAndName()
    {
#ifdef __linux__
        std::vector<char> buffer(4096);
        ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (len != -1)
        {
            buffer[static_cast<size_t>(len)] = '\0';
            return std::string(buffer.data());
        }
#endif

#ifdef _WIN32
        std::vector<char> buffer(MAX_PATH);
        DWORD len = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len > 0 && len < buffer.size())
        {
            return std::string(buffer.data(), len);
        }
#endif

#ifdef __APPLE__
        std::vector<char> buffer(4096);
        uint32_t size = static_cast<uint32_t>(buffer.size());
        if (_NSGetExecutablePath(buffer.data(), &size) == 0)
        {
            return std::string(buffer.data());
        }
#endif

        return {};
    }

    std::string getExecutablePath()
    {
        std::string fullPath = getExecutablePathAndName();
        if (!fullPath.empty())
        {
            std::filesystem::path p(fullPath);
            return p.parent_path().string() + "/";
        }
        return "./";
    }

    std::string snakeCaseToLowerCamelCase(const std::string &snakeCase)
    {
        if (snakeCase.empty())
        {
            return snakeCase;
        }

        std::string result;
        result.reserve(snakeCase.size()); // Reserve space to avoid reallocations

        bool capitalizeNext = false;

        for (char ch : snakeCase)
        {
            if (ch == '_')
            {
                // Mark that the next character should be capitalized
                capitalizeNext = true;
            }
            else
            {
                if (capitalizeNext)
                {
                    // Capitalize this character
                    result += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    capitalizeNext = false;
                }
                else
                {
                    // Keep the character as-is
                    result += ch;
                }
            }
        }

        return result;
    }

} // namespace cpp_dbc::system_utils
