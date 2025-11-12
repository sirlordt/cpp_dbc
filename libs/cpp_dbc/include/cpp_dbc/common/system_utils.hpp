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
#include <vector>

namespace cpp_dbc
{

    namespace system_utils
    {

        // Declare the mutex as external
        extern std::mutex global_cout_mutex;

        struct StackFrame
        {
            std::string file;
            int line;
            std::string function;
        };

        // Thread-safe print function
        inline void safePrint(const std::string &mark, const std::string &message)
        {
            std::lock_guard<std::mutex> lock(global_cout_mutex);
            std::cout << mark << ": " << message << std::endl;
        }

        inline std::string currentTimeMillis()
        {
            using namespace std::chrono;

            // Get current system time
            auto now = system_clock::now();

            // Convert to time_t for formatting hours/minutes/seconds
            std::time_t now_c = system_clock::to_time_t(now);
            std::tm tm = *std::localtime(&now_c);

            // Extract milliseconds
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

            // Format to string
            std::ostringstream oss;
            oss << std::put_time(&tm, "%H:%M:%S")
                << '.' << std::setw(3) << std::setfill('0') << ms.count();

            return oss.str();
        }

        // bool shouldSkipFrame(const std::string &filename, const std::string &function);
        std::vector<StackFrame> captureCallStack(bool captureAll = false, int skip = 1);
        void printCallStack(const std::vector<StackFrame> &frames);
    }

}

#endif // SYSTEM_UTILS_HPP
