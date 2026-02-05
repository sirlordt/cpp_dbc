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
#include <vector>
#include "cpp_dbc/backward.hpp"

namespace cpp_dbc::system_utils
{

        // Define the global mutex
        std::mutex global_cout_mutex; // NOSONAR - Mutex cannot be const as it needs to be locked/unlocked

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

        std::vector<StackFrame> captureCallStack(bool captureAll, int skip) noexcept
        {
            try
            {
                backward::StackTrace st;
                st.load_here(32);
                backward::TraceResolver tr;
                tr.load_stacktrace(st);

                std::vector<StackFrame> frames;

                for (size_t i = skip; i < st.size(); ++i)
                {
                    backward::ResolvedTrace trace = tr.resolve(st[i]);

                    std::string filename = trace.source.filename;
                    std::string function = trace.object_function;

                    if (captureAll == false && shouldSkipFrame(filename, function))
                    {
                        continue;
                    }

                    StackFrame frame;
                    frame.file = filename.empty() ? "??" : filename;
                    frame.line = trace.source.line;
                    frame.function = function.empty() ? "??" : function;

                    frames.push_back(frame);
                }

                return frames;
            }
            catch (...)
            {
                // Return empty vector if stack capture fails - noexcept guarantee
                return {};
            }
        }

        void printCallStack(const std::vector<StackFrame> &frames)
        {
            std::cout << "Stack trace (" << frames.size() << " frames):\n";
            for (size_t i = 0; i < frames.size(); ++i)
            {
                std::cout << "  #" << i << " "
                          << frames[i].file << ":" << frames[i].line
                          << " in " << frames[i].function << "\n";
            }
        }

        bool parseDBURL(const std::string &url,
                        const std::string &expectedPrefix,
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
                std::string rest = url.substr(expectedPrefix.length());

                // Check for empty rest
                if (rest.empty())
                {
                    return !requireDatabase;
                }

                // Check for local connection (starts with '/')
                if (rest[0] == '/')
                {
                    if (!allowLocalConnection)
                    {
                        // For non-local drivers, '/' indicates start of database path
                        // This is handled below in the normal parsing flow
                    }
                    else
                    {
                        // Local connection: prefix:///path/to/database
                        result.isLocal = true;
                        result.host = "";
                        result.database = rest;
                        return !requireDatabase || !result.database.empty();
                    }
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

} // namespace cpp_dbc::system_utils
