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

namespace cpp_dbc
{

    namespace system_utils
    {

        // Define the global mutex
        std::mutex global_cout_mutex;

        bool shouldSkipFrame(const std::string &filename, const std::string &function)
        {

            // Skip backward-cpp internals
            if (filename.find("backward.hpp") != std::string::npos)
            {
                return true;
            }

            // Skip libc/system frames
            if (filename.find("libc") != std::string::npos ||
                filename.find("csu/") != std::string::npos ||
                filename.find("sysdeps/") != std::string::npos)
            {
                return true;
            }

            // Skip system functions
            if (function.find("__libc") != std::string::npos ||
                function.find("_start") == 0 ||
                function == "??")
            { // <-- Esta línea ya lo debería filtrar
                return true;
            }

            // Skip unresolved frames (??:0)
            if (filename == "??" || filename.empty())
            { // <-- Agregar esta línea
                return true;
            }

            // Skip our own tracing infrastructure
            if (function.find("captureCallStack") != std::string::npos ||
                function.find("printCallStack") != std::string::npos)
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

    }

}
