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
        std::mutex global_cout_mutex;

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

        std::vector<StackFrame> captureCallStack(bool captureAll, int skip)
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

        // getCurrentTimestamp and logWithTimestamp are already defined as inline in the header file
        // No need to redefine them here

} // namespace cpp_dbc::system_utils
