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

 @file streams.hpp
 @brief Base stream and blob classes for cpp_dbc

*/

#ifndef CPP_DBC_CORE_STREAMS_HPP
#define CPP_DBC_CORE_STREAMS_HPP

#include <cstdint>
#include <memory>
#include <vector>

namespace cpp_dbc
{

    // Abstract base class for input streams
    class InputStream
    {
    public:
        virtual ~InputStream() = default;

        // Read up to length bytes into the buffer
        // Returns the number of bytes actually read, or -1 if end of stream
        virtual int read(uint8_t *buffer, size_t length) = 0;

        // Skip n bytes
        virtual void skip(size_t n) = 0;

        // Close the stream
        virtual void close() = 0;
    };

    // Abstract base class for output streams
    class OutputStream
    {
    public:
        virtual ~OutputStream() = default;

        // Write length bytes from the buffer
        virtual void write(const uint8_t *buffer, size_t length) = 0;

        // Flush any buffered data
        virtual void flush() = 0;

        // Close the stream
        virtual void close() = 0;
    };

    // Abstract base class for BLOB objects
    class Blob
    {
    public:
        virtual ~Blob() = default;

        // Get the length of the BLOB
        virtual size_t length() const = 0;

        // Get a portion of the BLOB as a vector of bytes
        virtual std::vector<uint8_t> getBytes(size_t pos, size_t length) const = 0;

        // Get a stream to read from the BLOB
        virtual std::shared_ptr<InputStream> getBinaryStream() const = 0;

        // Get a stream to write to the BLOB starting at position pos
        virtual std::shared_ptr<OutputStream> setBinaryStream(size_t pos) = 0;

        // Write bytes to the BLOB starting at position pos
        virtual void setBytes(size_t pos, const std::vector<uint8_t> &bytes) = 0;

        // Write bytes to the BLOB starting at position pos
        virtual void setBytes(size_t pos, const uint8_t *bytes, size_t length) = 0;

        // Truncate the BLOB to the specified length
        virtual void truncate(size_t len) = 0;

        // Free resources associated with the BLOB
        virtual void free() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_STREAMS_HPP
