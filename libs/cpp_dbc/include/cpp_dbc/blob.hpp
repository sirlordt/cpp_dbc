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

 @file blob.hpp
 @brief Tests for BLOB operations

*/

#ifndef CPP_DBC_BLOB_HPP
#define CPP_DBC_BLOB_HPP

#include "cpp_dbc.hpp"
#include <fstream>
#include <sstream>
#include <cstring>

namespace cpp_dbc
{
    // Base implementation of InputStream that reads from a memory buffer
    class MemoryInputStream : public InputStream
    {
    private:
        const std::vector<uint8_t> &m_data;
        size_t m_position{0};

    public:
        explicit MemoryInputStream(const std::vector<uint8_t> &data)
            : m_data(data) {}

        int read(uint8_t *buffer, size_t length) override
        {
            if (m_position >= m_data.size())
                return -1; // End of stream

            size_t bytesToRead = std::min(length, m_data.size() - m_position);
            std::memcpy(buffer, m_data.data() + m_position, bytesToRead);
            m_position += bytesToRead;
            return static_cast<int>(bytesToRead);
        }

        void skip(size_t n) override
        {
            m_position = std::min(m_position + n, m_data.size());
        }

        void close() override
        {
            // Nothing to do for memory stream
        }
    };

    // Base implementation of OutputStream that writes to a memory buffer
    class MemoryOutputStream : public OutputStream
    {
    private:
        std::vector<uint8_t> &m_data;
        size_t m_position{0};

    public:
        MemoryOutputStream(std::vector<uint8_t> &data, size_t position)
            : m_data(data), m_position(position) {}

        void write(const uint8_t *buffer, size_t length) override
        {
            // Ensure the vector is large enough
            if (m_position + length > m_data.size())
                m_data.resize(m_position + length);

            // Copy the data
            std::memcpy(m_data.data() + m_position, buffer, length);
            m_position += length;
        }

        void flush() override
        {
            // Nothing to do for memory stream
        }

        void close() override
        {
            // Nothing to do for memory stream
        }
    };

    // Base implementation of InputStream that reads from a file
    class FileInputStream : public InputStream
    {
    private:
        std::ifstream file;

    public:
        explicit FileInputStream(const std::string &filename)
            : file(filename, std::ios::binary)
        {
            if (!file)
                throw DBException("FE66975AE75B", "Failed to open file for reading: " + filename, system_utils::captureCallStack());
        }

        int read(uint8_t *buffer, size_t length) override
        {
            if (!file || file.eof())
                return -1; // End of stream

            file.read(reinterpret_cast<char *>(buffer), length);
            return static_cast<int>(file.gcount());
        }

        void skip(size_t n) override
        {
            file.seekg(n, std::ios::cur);
        }

        void close() override
        {
            file.close();
        }
    };

    // Base implementation of OutputStream that writes to a file
    class FileOutputStream : public OutputStream
    {
    private:
        std::ofstream file;

    public:
        explicit FileOutputStream(const std::string &filename, bool append = false)
            : file(filename, append ? (std::ios::binary | std::ios::app) : (std::ios::binary | std::ios::trunc))
        {
            if (!file)
                throw DBException("DFD87502D621", "Failed to open file for writing: " + filename, system_utils::captureCallStack());
        }

        void write(const uint8_t *buffer, size_t length) override
        {
            file.write(reinterpret_cast<const char *>(buffer), length);
            if (!file)
                throw DBException("A5060D0FEA5A", "Failed to write to file", system_utils::captureCallStack());
        }

        void flush() override
        {
            file.flush();
        }

        void close() override
        {
            file.close();
        }
    };

    // Base implementation of Blob that stores data in memory
    class MemoryBlob : public Blob
    {
    protected:
        std::vector<uint8_t> m_data;

    public:
        MemoryBlob() = default;

        explicit MemoryBlob(const std::vector<uint8_t> &initialData)
            : m_data(initialData) {}

        explicit MemoryBlob(std::vector<uint8_t> &&initialData)
            : m_data(std::move(initialData)) {}

        size_t length() const override
        {
            return m_data.size();
        }

        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
        {
            if (pos >= m_data.size())
                return {};

            size_t bytesToRead = std::min(length, m_data.size() - pos);
            return std::vector<uint8_t>(m_data.begin() + pos, m_data.begin() + pos + bytesToRead);
        }

        std::shared_ptr<InputStream> getBinaryStream() const override
        {
            return std::make_shared<MemoryInputStream>(m_data);
        }

        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
        {
            return std::make_shared<MemoryOutputStream>(m_data, pos);
        }

        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
        {
            setBytes(pos, bytes.data(), bytes.size());
        }

        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
        {
            if (pos + length > m_data.size())
                m_data.resize(pos + length);

            std::memcpy(m_data.data() + pos, bytes, length);
        }

        void truncate(size_t len) override
        {
            if (len < m_data.size())
                m_data.resize(len);
        }

        void free() override
        {
            m_data.clear();
            m_data.shrink_to_fit();
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_BLOB_HPP