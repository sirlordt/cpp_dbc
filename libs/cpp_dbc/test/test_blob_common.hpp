// test_blob_common.hpp
// Common utilities for BLOB testing
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

#pragma once

#include <vector>
#include <random>
#include <cstring>

// Helper function to generate random binary data
inline std::vector<uint8_t> generateRandomBinaryData(size_t size)
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
inline bool compareBinaryData(const std::vector<uint8_t> &data1, const std::vector<uint8_t> &data2)
{
    if (data1.size() != data2.size())
    {
        return false;
    }

    return std::memcmp(data1.data(), data2.data(), data1.size()) == 0;
}