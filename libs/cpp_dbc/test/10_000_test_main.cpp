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

 @file test_main.cpp
 @brief Implementation for the cpp_dbc library

*/

// This file is automatically included by Catch2WithMain
// No need to define CATCH_CONFIG_MAIN as we're using the pre-compiled main
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

#include "10_000_test_main.hpp"

namespace common_test_helpers
{
    // Helper function to generate random JSON data
    std::string generateRandomJson(int depth, int maxItems)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> itemDist(1, maxItems);
        static std::uniform_int_distribution<> typeDist(0, 5);
        static std::uniform_int_distribution<> intDist(-1000, 1000);
        static std::uniform_real_distribution<> floatDist(-1000.0, 1000.0);
        static const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        static std::uniform_int_distribution<> charDist(0, static_cast<int>(chars.size() - 1));

        if (depth <= 0)
        {
            // Generate a leaf value
            int type = typeDist(gen);
            switch (type)
            {
            case 0:
                return "null";
            case 1:
                return std::to_string(intDist(gen));
            case 2:
            {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << floatDist(gen);
                return ss.str();
            }
            case 3:
                return "true";
            case 4:
                return "false";
            default:
            {
                // Generate a random string
                int length = itemDist(gen) + 2;
                std::string s = "\"";
                for (int i = 0; i < length; i++)
                {
                    s += chars[charDist(gen)];
                }
                s += "\"";
                return s;
            }
            }
        }

        // Decide between object and array
        bool isObject = (typeDist(gen) % 2 == 0);

        std::stringstream json;
        if (isObject)
        {
            json << "{";
            int items = itemDist(gen);
            for (int i = 0; i < items; i++)
            {
                if (i > 0)
                    json << ",";
                // Generate a key
                json << "\"key" << i << "\":";
                // Generate a value or nested structure
                if (typeDist(gen) % 3 == 0 && depth > 1)
                {
                    json << generateRandomJson(depth - 1, maxItems);
                }
                else
                {
                    json << generateRandomJson(0, maxItems);
                }
            }
            json << "}";
        }
        else
        {
            json << "[";
            int items = itemDist(gen);
            for (int i = 0; i < items; i++)
            {
                if (i > 0)
                    json << ",";
                // Generate a value or nested structure
                if (typeDist(gen) % 3 == 0 && depth > 1)
                {
                    json << generateRandomJson(depth - 1, maxItems);
                }
                else
                {
                    json << generateRandomJson(0, maxItems);
                }
            }
            json << "]";
        }

        return json.str();
    }

    std::string getExecutablePathAndName()
    {
        std::vector<char> buffer(2048);
        ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (len != -1)
        {
            buffer[len] = '\0';
            return std::string(buffer.data());
        }
        return {};
    }

    std::string getOnlyExecutablePath()
    {
        std::filesystem::path p(getExecutablePathAndName());
        return p.parent_path().string() + "/"; // Devuelve solo el directorio
    }

    // Helper function to get the path to the test_db_connections.yml file
    std::string getConfigFilePath()
    {
        // Print current working directory for debugging
        // char cwd[1024];
        // if (getcwd(cwd, sizeof(cwd)) != NULL)
        //{
        // std::cout << "Current working directory: " << cwd << std::endl;
        //}

        // The YAML file is copied to the build directory by CMake
        // So we can just use the filename
        // std::cout << "Attempting to open file: test_db_connections.yml" << std::endl;
        return getOnlyExecutablePath() + "test_db_connections.yml";
    }

    // Helper function to read binary data from a file
    std::vector<uint8_t> readBinaryFile(const std::string &filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open file: " + filePath);
        }

        // Get file size
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read the data
        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
        {
            throw std::runtime_error("Failed to read file: " + filePath);
        }

        return buffer;
    }

    // Helper function to write binary data to a file
    void writeBinaryFile(const std::string &filePath, const std::vector<uint8_t> &data)
    {
        std::ofstream file(filePath, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to create file: " + filePath);
        }

        file.write(reinterpret_cast<const char *>(data.data()), data.size());
        if (!file)
        {
            throw std::runtime_error("Failed to write to file: " + filePath);
        }
    }

    // Helper function to get the path to the test.jpg file
    std::string getTestImagePath()
    {
        // Use the same approach as getConfigFilePath()
        // The test.jpg file is copied to the build directory by CMake
        return getOnlyExecutablePath() + "test.jpg";
    }

    // Helper function to generate a random filename in /tmp directory
    std::string generateRandomTempFilename()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(10000, 99999);

        return "/tmp/test_image_" + std::to_string(distrib(gen)) + ".jpg";
    }

    // Helper function to generate random binary data
    std::vector<uint8_t> generateRandomBinaryData(size_t size)
    {
        std::vector<uint8_t> data(size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 255);

        for (size_t i = 0; i < size; ++i)
        {
            data[i] = static_cast<uint8_t>(distrib(gen));
        }

        return data;
    }

    // Helper function to compare binary data
    bool compareBinaryData(const std::vector<uint8_t> &data1, const std::vector<uint8_t> &data2)
    {
        if (data1.size() != data2.size())
        {
            return false;
        }

        return std::memcmp(data1.data(), data2.data(), data1.size()) == 0;
    }

} // namespace common_test_helpers
