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

 @file test_blob_common.hpp
 @brief Tests for BLOB operations

*/

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