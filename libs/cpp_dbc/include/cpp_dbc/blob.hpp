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
 @brief BLOB stream implementations for memory and file-based binary data

*/

#ifndef CPP_DBC_BLOB_HPP
#define CPP_DBC_BLOB_HPP

#include "cpp_dbc/core/streams.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include <fstream>
#include <sstream>
#include <cstring>

namespace cpp_dbc
{
    /**
     * @brief InputStream implementation that reads from a memory buffer
     *
     * @note The referenced buffer must outlive the stream instance.
     *
     * ```cpp
     * std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
     * auto stream = std::make_shared<cpp_dbc::MemoryInputStream>(data);
     * uint8_t buf[5];
     * int bytesRead = stream->read(buf, 5);
     * ```
     */
    class MemoryInputStream : public InputStream
    {
    private:
        const std::vector<uint8_t> &m_data;
        size_t m_position{0};

    public:
        explicit MemoryInputStream(const std::vector<uint8_t> &data)
            : m_data(data) {}

        static cpp_dbc::expected<std::shared_ptr<MemoryInputStream>, DBException> create(std::nothrow_t, const std::vector<uint8_t> &data) noexcept
        {
            return std::make_shared<MemoryInputStream>(data);
        }

        static std::shared_ptr<MemoryInputStream> create(const std::vector<uint8_t> &data)
        {
            auto r = create(std::nothrow, data);
            if (!r.has_value()) { throw r.error(); }
            return r.value();
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

        #ifdef __cpp_exceptions

        int read(uint8_t *buffer, size_t length) override
        {
            auto r = read(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void skip(size_t n) override
        {
            auto r = skip(std::nothrow, n);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void close() override
        {
            auto r = close(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<int, DBException> read(std::nothrow_t, uint8_t *buffer, size_t length) noexcept override
        {
            if (m_position >= m_data.size())
            {
                return -1; // End of stream
            }
            size_t bytesToRead = std::min(length, m_data.size() - m_position);
            std::memcpy(buffer, m_data.data() + m_position, bytesToRead);
            m_position += bytesToRead;
            return static_cast<int>(bytesToRead);
        }

        cpp_dbc::expected<void, DBException> skip(std::nothrow_t, size_t n) noexcept override
        {
            m_position = std::min(m_position + n, m_data.size());
            return {};
        }

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override
        {
            // Nothing to do for memory stream
            return {};
        }
    };

    /**
     * @brief OutputStream implementation that writes to a memory buffer
     *
     * @note The referenced buffer must outlive the stream instance.
     *
     * ```cpp
     * std::vector<uint8_t> data;
     * auto stream = std::make_shared<cpp_dbc::MemoryOutputStream>(data, 0);
     * uint8_t buf[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
     * stream->write(buf, 5); // data now contains "Hello"
     * ```
     */
    class MemoryOutputStream : public OutputStream
    {
    private:
        std::vector<uint8_t> &m_data;
        size_t m_position{0};

    public:
        MemoryOutputStream(std::vector<uint8_t> &data, size_t position)
            : m_data(data), m_position(position) {}

        static cpp_dbc::expected<std::shared_ptr<MemoryOutputStream>, DBException> create(std::nothrow_t, std::vector<uint8_t> &data, size_t position) noexcept
        {
            return std::make_shared<MemoryOutputStream>(data, position);
        }

        static std::shared_ptr<MemoryOutputStream> create(std::vector<uint8_t> &data, size_t position)
        {
            auto r = create(std::nothrow, data, position);
            if (!r.has_value()) { throw r.error(); }
            return r.value();
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

        #ifdef __cpp_exceptions

        void write(const uint8_t *buffer, size_t length) override
        {
            auto r = write(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void flush() override
        {
            auto r = flush(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void close() override
        {
            auto r = close(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<void, DBException> write(std::nothrow_t, const uint8_t *buffer, size_t length) noexcept override
        {
            if (m_position + length > m_data.size())
            {
                m_data.resize(m_position + length);
            }
            std::memcpy(m_data.data() + m_position, buffer, length);
            m_position += length;
            return {};
        }

        cpp_dbc::expected<void, DBException> flush(std::nothrow_t) noexcept override
        {
            // Nothing to do for memory stream
            return {};
        }

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override
        {
            // Nothing to do for memory stream
            return {};
        }
    };

    /**
     * @brief InputStream implementation that reads from a file
     *
     * ```cpp
     * auto stream = std::make_shared<cpp_dbc::FileInputStream>("/path/to/file.bin");
     * uint8_t buf[1024];
     * int bytesRead = stream->read(buf, sizeof(buf));
     * stream->close();
     * ```
     *
     * @throws DBException if the file cannot be opened
     */
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

        static cpp_dbc::expected<std::shared_ptr<FileInputStream>, DBException> create(std::nothrow_t, const std::string &filename) noexcept
        {
            try
            {
                return std::make_shared<FileInputStream>(filename);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("TQK5QFXF7GOI", ex.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("PCGKFNDJKO6B", "Unknown error creating FileInputStream", system_utils::captureCallStack()));
            }
        }

        static std::shared_ptr<FileInputStream> create(const std::string &filename)
        {
            auto r = create(std::nothrow, filename);
            if (!r.has_value()) { throw r.error(); }
            return r.value();
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

        #ifdef __cpp_exceptions

        int read(uint8_t *buffer, size_t length) override
        {
            auto r = read(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void skip(size_t n) override
        {
            auto r = skip(std::nothrow, n);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void close() override
        {
            auto r = close(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<int, DBException> read(std::nothrow_t, uint8_t *buffer, size_t length) noexcept override
        {
            if (!file || file.eof())
            {
                return -1; // End of stream
            }
            file.read(reinterpret_cast<char *>(buffer), length);
            return static_cast<int>(file.gcount());
        }

        cpp_dbc::expected<void, DBException> skip(std::nothrow_t, size_t n) noexcept override
        {
            if (!file.good())
            {
                return cpp_dbc::unexpected(DBException("O8E1N1QYPY9E", "Cannot skip in file stream: stream is in bad state", system_utils::captureCallStack()));
            }
            file.seekg(static_cast<std::streamoff>(n), std::ios::cur);
            if (file.fail() && !file.eof())
            {
                return cpp_dbc::unexpected(DBException("UDCL5NMOKZ4H", "Failed to skip in file stream", system_utils::captureCallStack()));
            }
            return {};
        }

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override
        {
            file.close();
            return {};
        }
    };

    /**
     * @brief OutputStream implementation that writes to a file
     *
     * ```cpp
     * auto stream = std::make_shared<cpp_dbc::FileOutputStream>("/path/to/output.bin");
     * uint8_t buf[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
     * stream->write(buf, 5);
     * stream->flush();
     * stream->close();
     * ```
     *
     * @throws DBException if the file cannot be opened
     */
    class FileOutputStream : public OutputStream
    {
    private:
        std::ofstream file;

    public:
        explicit FileOutputStream(const std::string &filename, bool append = false)
            : file(filename, append ? (std::ios::binary | std::ios::app) : (std::ios::binary | std::ios::trunc))
        {
            if (!file)
                throw DBException("79BOO1ZI630P", "Failed to open file for writing: " + filename, system_utils::captureCallStack());
        }

        static cpp_dbc::expected<std::shared_ptr<FileOutputStream>, DBException> create(std::nothrow_t, const std::string &filename, bool append = false) noexcept
        {
            try
            {
                return std::make_shared<FileOutputStream>(filename, append);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("T52C3FOHYPKU", ex.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("945M5YZM3UCN", "Unknown error creating FileOutputStream", system_utils::captureCallStack()));
            }
        }

        static std::shared_ptr<FileOutputStream> create(const std::string &filename, bool append = false)
        {
            auto r = create(std::nothrow, filename, append);
            if (!r.has_value()) { throw r.error(); }
            return r.value();
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

        #ifdef __cpp_exceptions

        void write(const uint8_t *buffer, size_t length) override
        {
            auto r = write(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void flush() override
        {
            auto r = flush(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void close() override
        {
            auto r = close(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<void, DBException> write(std::nothrow_t, const uint8_t *buffer, size_t length) noexcept override
        {
            file.write(reinterpret_cast<const char *>(buffer), length);
            if (!file)
            {
                return cpp_dbc::unexpected(DBException("2IFNPC72PVE8", "Failed to write to file", system_utils::captureCallStack()));
            }
            return {};
        }

        cpp_dbc::expected<void, DBException> flush(std::nothrow_t) noexcept override
        {
            file.flush();
            return {};
        }

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override
        {
            file.close();
            return {};
        }
    };

    /**
     * @brief Blob implementation that stores binary data in memory
     *
     * @note Streams returned by getBinaryStream() and setBinaryStream() hold references
     *       to internal data and become invalid if the MemoryBlob is destroyed.
     *
     * ```cpp
     * std::vector<uint8_t> data = {0x01, 0x02, 0x03};
     * cpp_dbc::MemoryBlob blob(data);
     * auto bytes = blob.getBytes(0, blob.length());
     * auto stream = blob.getBinaryStream();
     * ```
     *
     * @see InputStream, OutputStream
     */
    class MemoryBlob : public Blob
    {
    protected:
        // mutable is required here for lazy loading in derived classes (MySQLBlob, PostgreSQLBlob, etc.)
        // Their ensureLoaded() const methods need to populate this cache on first access
        mutable std::vector<uint8_t> m_data;

    public:
        MemoryBlob() = default;

        explicit MemoryBlob(const std::vector<uint8_t> &initialData)
            : m_data(initialData) {}

        explicit MemoryBlob(std::vector<uint8_t> &&initialData)
            : m_data(std::move(initialData)) {}

        static cpp_dbc::expected<std::shared_ptr<MemoryBlob>, DBException> create(std::nothrow_t) noexcept
        {
            return std::make_shared<MemoryBlob>();
        }

        static cpp_dbc::expected<std::shared_ptr<MemoryBlob>, DBException> create(std::nothrow_t, const std::vector<uint8_t> &initialData) noexcept
        {
            return std::make_shared<MemoryBlob>(initialData);
        }

        static cpp_dbc::expected<std::shared_ptr<MemoryBlob>, DBException> create(std::nothrow_t, std::vector<uint8_t> &&initialData) noexcept
        {
            return std::make_shared<MemoryBlob>(std::move(initialData));
        }

        static std::shared_ptr<MemoryBlob> create()
        {
            auto r = create(std::nothrow);
            if (!r.has_value()) { throw r.error(); }
            return r.value();
        }

        static std::shared_ptr<MemoryBlob> create(const std::vector<uint8_t> &initialData)
        {
            auto r = create(std::nothrow, initialData);
            if (!r.has_value()) { throw r.error(); }
            return r.value();
        }

        static std::shared_ptr<MemoryBlob> create(std::vector<uint8_t> &&initialData)
        {
            auto r = create(std::nothrow, std::move(initialData));
            if (!r.has_value()) { throw r.error(); }
            return r.value();
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const MemoryBlob &other) noexcept
        {
            if (this == &other) { return {}; }
            m_data = other.m_data;
            return {};
        }

        void copyFrom(const MemoryBlob &other)
        {
            auto r = copyFrom(std::nothrow, other);
            if (!r.has_value()) { throw r.error(); }
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

        #ifdef __cpp_exceptions

        size_t length() const override
        {
            auto r = length(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
        {
            auto r = getBytes(std::nothrow, pos, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::shared_ptr<InputStream> getBinaryStream() const override
        {
            auto r = getBinaryStream(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
        {
            auto r = setBinaryStream(std::nothrow, pos);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
        {
            auto r = setBytes(std::nothrow, pos, bytes);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
        {
            auto r = setBytes(std::nothrow, pos, bytes, length);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void truncate(size_t len) override
        {
            auto r = truncate(std::nothrow, len);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void free() override
        {
            auto r = free(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override
        {
            return m_data.size();
        }

        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override
        {
            if (pos >= m_data.size())
            {
                return std::vector<uint8_t>{};
            }
            size_t bytesToRead = std::min(length, m_data.size() - pos);
            return std::vector<uint8_t>(m_data.begin() + pos, m_data.begin() + pos + bytesToRead);
        }

        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override
        {
            return std::shared_ptr<InputStream>(std::make_shared<MemoryInputStream>(m_data));
        }

        cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override
        {
            return std::shared_ptr<OutputStream>(std::make_shared<MemoryOutputStream>(m_data, pos));
        }

        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override
        {
            return setBytes(std::nothrow, pos, bytes.data(), bytes.size());
        }

        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override
        {
            if (pos + length > m_data.size())
            {
                m_data.resize(pos + length);
            }
            std::memcpy(m_data.data() + pos, bytes, length);
            return {};
        }

        cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override
        {
            if (len < m_data.size())
            {
                m_data.resize(len);
            }
            return {};
        }

        cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override
        {
            m_data.clear();
            m_data.shrink_to_fit();
            return {};
        }

        /**
         * @brief In-memory BLOBs have no database connection — always valid
         */
        bool isConnectionValid() const noexcept override
        {
            return true;
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_BLOB_HPP