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

 @file 10_000_test_main.hpp
 @brief Common test helpers for the cpp_dbc library

*/

#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <random>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cpp_dbc/common/system_utils.hpp>

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif

namespace common_test_helpers
{
    // Helper function to generate random JSON data
    std::string generateRandomJson(int depth = 3, int maxItems = 5);

    // Helper function to get the path to the test_db_connections.yml file
    std::string getConfigFilePath();

    // Helper function to read binary data from a file
    std::vector<uint8_t> readBinaryFile(const std::string &filePath);

    // Helper function to write binary data to a file
    void writeBinaryFile(const std::string &filePath, const std::vector<uint8_t> &data);

    // Helper function to get the path to the test.jpg file
    std::string getTestImagePath();

    // Helper function to generate a random filename in /tmp directory
    std::string generateRandomTempFilename();

    // Helper function to generate random binary data
    std::vector<uint8_t> generateRandomBinaryData(size_t size);

    // Helper function to compare binary data
    bool compareBinaryData(const std::vector<uint8_t> &data1, const std::vector<uint8_t> &data2);

} // namespace common_test_helpers