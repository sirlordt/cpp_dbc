// test_main.cpp
// Main test file for the cpp_dbc project
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

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
