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
 * @file benchmark_main.cpp
 * @brief Main file for cpp_dbc benchmarks using Catch2
 */

// This file is automatically included by Catch2WithMain
// No need to define CATCH_CONFIG_MAIN as we're using the pre-compiled main
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <random>
#include <cstring>
#include <sstream>
#include <iomanip>

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath()
{
    std::vector<char> buffer(2048);
    ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (len != -1)
    {
        buffer[len] = '\0';
        std::filesystem::path p(std::string(buffer.data()));
        return p.parent_path().string() + "/test_db_connections.yml";
    }
    return "test_db_connections.yml"; // Fallback
}