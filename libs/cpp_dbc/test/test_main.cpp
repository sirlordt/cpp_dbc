// This file is automatically included by Catch2WithMain
// No need to define CATCH_CONFIG_MAIN as we're using the pre-compiled main
#include <string>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

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
