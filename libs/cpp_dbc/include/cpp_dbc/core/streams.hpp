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

    /**
     * @brief Abstract base class for binary input streams
     *
     * Used to read binary data (BLOBs) from database result sets.
     *
     * ```cpp
     * auto stream = rs->getBinaryStream("photo");
     * std::vector<uint8_t> buf(4096);
     * int n;
     * while ((n = stream->read(buf.data(), buf.size())) > 0) {
     *     // process n bytes from buf
     * }
     * stream->close();
     * ```
     *
     * @see Blob, RelationalDBResultSet::getBinaryStream
     */
    class InputStream
    {
    public:
        virtual ~InputStream() = default;

        /**
         * @brief Read up to length bytes into the buffer
         *
         * @param buffer Pointer to the destination buffer
         * @param length Maximum number of bytes to read
         * @return Number of bytes actually read, or -1 if end of stream
         */
        virtual int read(uint8_t *buffer, size_t length) = 0;

        /**
         * @brief Skip n bytes in the stream
         * @param n Number of bytes to skip
         */
        virtual void skip(size_t n) = 0;

        /**
         * @brief Close the stream and release resources
         */
        virtual void close() = 0;
    };

    /**
     * @brief Abstract base class for binary output streams
     *
     * Used to write binary data into BLOBs.
     *
     * ```cpp
     * auto out = blob->setBinaryStream(0);
     * std::vector<uint8_t> data = {0x89, 0x50, 0x4E, 0x47};
     * out->write(data.data(), data.size());
     * out->flush();
     * out->close();
     * ```
     *
     * @see Blob::setBinaryStream
     */
    class OutputStream
    {
    public:
        virtual ~OutputStream() = default;

        /**
         * @brief Write length bytes from the buffer
         *
         * @param buffer Pointer to the data to write
         * @param length Number of bytes to write
         */
        virtual void write(const uint8_t *buffer, size_t length) = 0;

        /**
         * @brief Flush any buffered data to the underlying storage
         */
        virtual void flush() = 0;

        /**
         * @brief Close the stream and release resources
         */
        virtual void close() = 0;
    };

    /**
     * @brief Abstract base class for Binary Large Object (BLOB) handling
     *
     * Represents binary data stored in a database column. Provides both
     * random-access byte operations and stream-based I/O.
     *
     * ```cpp
     * // Reading a BLOB from a result set
     * auto blob = rs->getBlob("image_data");
     * auto bytes = blob->getBytes(0, blob->length());
     * ```
     *
     * ```cpp
     * // Writing a BLOB via prepared statement
     * // User-defined function to load file contents into a byte vector
     * std::vector<uint8_t> imageData; // = yourLoadFileFunction("photo.png");
     * imageData = {0x89, 0x50, 0x4E, 0x47}; // PNG magic bytes as example
     * stmt->setBytes(1, imageData);
     * stmt->executeUpdate();
     * ```
     *
     * @see RelationalDBPreparedStatement::setBlob, RelationalDBResultSet::getBlob
     */
    class Blob
    {
    public:
        virtual ~Blob() = default;

        /**
         * @brief Get the length of the BLOB in bytes
         * @return The number of bytes in the BLOB
         */
        virtual size_t length() const = 0;

        /**
         * @brief Get a portion of the BLOB as a byte vector
         *
         * @param pos Starting position (0-based)
         * @param length Number of bytes to read
         * @return Vector containing the requested bytes
         */
        virtual std::vector<uint8_t> getBytes(size_t pos, size_t length) const = 0;

        /**
         * @brief Get an input stream to read the BLOB data
         * @return A shared pointer to an InputStream for reading
         */
        virtual std::shared_ptr<InputStream> getBinaryStream() const = 0;

        /**
         * @brief Get an output stream to write to the BLOB starting at position pos
         *
         * @param pos Starting write position (0-based)
         * @return A shared pointer to an OutputStream for writing
         */
        virtual std::shared_ptr<OutputStream> setBinaryStream(size_t pos) = 0;

        /**
         * @brief Write bytes to the BLOB starting at position pos
         *
         * @param pos Starting write position (0-based)
         * @param bytes The bytes to write
         */
        virtual void setBytes(size_t pos, const std::vector<uint8_t> &bytes) = 0;

        /**
         * @brief Write bytes from a raw pointer to the BLOB
         *
         * @param pos Starting write position (0-based)
         * @param bytes Pointer to the data
         * @param length Number of bytes to write
         */
        virtual void setBytes(size_t pos, const uint8_t *bytes, size_t length) = 0;

        /**
         * @brief Truncate the BLOB to the specified length
         * @param len The new length in bytes
         */
        virtual void truncate(size_t len) = 0;

        /**
         * @brief Free resources associated with the BLOB
         */
        virtual void free() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_STREAMS_HPP
