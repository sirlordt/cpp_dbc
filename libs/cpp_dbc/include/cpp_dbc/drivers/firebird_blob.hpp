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

 @file firebird_blob.hpp
 @brief BLOB support for Firebird database operations

*/

#ifndef CPP_DBC_FIREBIRD_BLOB_HPP
#define CPP_DBC_FIREBIRD_BLOB_HPP

#include "../cpp_dbc.hpp"
#include "../blob.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0 // Default to disabled
#endif

#if USE_FIREBIRD
#include <ibase.h>

namespace cpp_dbc
{
    namespace Firebird
    {
        // Firebird implementation of InputStream
        class FirebirdInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position;

        public:
            FirebirdInputStream(const void *buffer, size_t length)
                : m_data(static_cast<const uint8_t *>(buffer), static_cast<const uint8_t *>(buffer) + length), m_position(0) {}

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

        // Forward declaration for weak_ptr usage
        class FirebirdConnection;

        // Firebird implementation of Blob
        class FirebirdBlob : public MemoryBlob
        {
        private:
            isc_db_handle *m_db;
            isc_tr_handle *m_tr;
            ISC_QUAD m_blobId;
            bool m_loaded;
            bool m_hasValidId;

        public:
            // Constructor for creating a new BLOB
            FirebirdBlob(isc_db_handle *db, isc_tr_handle *tr)
                : m_db(db), m_tr(tr), m_loaded(true), m_hasValidId(false)
            {
                m_blobId.gds_quad_high = 0;
                m_blobId.gds_quad_low = 0;
            }

            // Constructor for loading an existing BLOB by ID
            FirebirdBlob(isc_db_handle *db, isc_tr_handle *tr, ISC_QUAD blobId)
                : m_db(db), m_tr(tr), m_blobId(blobId), m_loaded(false), m_hasValidId(true) {}

            // Constructor for creating a BLOB from existing data
            FirebirdBlob(isc_db_handle *db, isc_tr_handle *tr, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), m_db(db), m_tr(tr), m_loaded(true), m_hasValidId(false)
            {
                m_blobId.gds_quad_high = 0;
                m_blobId.gds_quad_low = 0;
            }

            // Load the BLOB data from the database if not already loaded
            void ensureLoaded()
            {
                if (m_loaded || !m_hasValidId)
                    return;

                ISC_STATUS_ARRAY status;
                isc_blob_handle blobHandle = 0;

                // Open the blob
                if (isc_open_blob2(status, m_db, m_tr, &blobHandle, &m_blobId, 0, nullptr))
                {
                    throw DBException("K3M7N9P2Q5R8", "Failed to open BLOB for reading", system_utils::captureCallStack());
                }

                // Read the blob data in chunks
                const size_t CHUNK_SIZE = 32768;
                std::vector<uint8_t> buffer(CHUNK_SIZE);
                unsigned short actualLength;
                ISC_STATUS blobStatus;

                m_data.clear();

                while (true)
                {
                    blobStatus = isc_get_segment(status, &blobHandle, &actualLength,
                                                 static_cast<unsigned short>(CHUNK_SIZE),
                                                 reinterpret_cast<char *>(buffer.data()));

                    if (blobStatus == isc_segstr_eof)
                    {
                        break;
                    }

                    if (blobStatus && blobStatus != isc_segment)
                    {
                        isc_close_blob(status, &blobHandle);
                        throw DBException("L4N8P0Q6R2S9", "Failed to read BLOB segment", system_utils::captureCallStack());
                    }

                    m_data.insert(m_data.end(), buffer.begin(), buffer.begin() + actualLength);
                }

                // Close the blob
                if (isc_close_blob(status, &blobHandle))
                {
                    throw DBException("M5P9Q1R7S3T0", "Failed to close BLOB after reading", system_utils::captureCallStack());
                }

                m_loaded = true;
            }

            // Override methods that need to ensure the BLOB is loaded
            size_t length() const override
            {
                const_cast<FirebirdBlob *>(this)->ensureLoaded();
                return MemoryBlob::length();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                const_cast<FirebirdBlob *>(this)->ensureLoaded();
                return MemoryBlob::getBytes(pos, length);
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                const_cast<FirebirdBlob *>(this)->ensureLoaded();
                return MemoryBlob::getBinaryStream();
            }

            std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
            {
                ensureLoaded();
                return MemoryBlob::setBinaryStream(pos);
            }

            void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
            {
                ensureLoaded();
                MemoryBlob::setBytes(pos, bytes);
            }

            void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
            {
                ensureLoaded();
                MemoryBlob::setBytes(pos, bytes, length);
            }

            void truncate(size_t len) override
            {
                ensureLoaded();
                MemoryBlob::truncate(len);
            }

            // Save the BLOB data to the database and return the blob ID
            ISC_QUAD save()
            {
                ISC_STATUS_ARRAY status;
                isc_blob_handle blobHandle = 0;

                // Create a new blob
                if (isc_create_blob2(status, m_db, m_tr, &blobHandle, &m_blobId, 0, nullptr))
                {
                    throw DBException("N6Q0R2S8T4U1", "Failed to create BLOB for writing", system_utils::captureCallStack());
                }

                // Write the data in chunks
                const size_t CHUNK_SIZE = 32768;
                size_t offset = 0;

                while (offset < m_data.size())
                {
                    size_t chunkSize = std::min(CHUNK_SIZE, m_data.size() - offset);

                    if (isc_put_segment(status, &blobHandle,
                                        static_cast<unsigned short>(chunkSize),
                                        reinterpret_cast<const char *>(m_data.data() + offset)))
                    {
                        isc_close_blob(status, &blobHandle);
                        throw DBException("P7R1S3T9U5V2", "Failed to write BLOB segment", system_utils::captureCallStack());
                    }

                    offset += chunkSize;
                }

                // Close the blob
                if (isc_close_blob(status, &blobHandle))
                {
                    throw DBException("Q8S2T4U0V6W3", "Failed to close BLOB after writing", system_utils::captureCallStack());
                }

                m_hasValidId = true;
                return m_blobId;
            }

            // Get the BLOB ID
            ISC_QUAD getBlobId() const
            {
                return m_blobId;
            }

            // Check if the blob has a valid ID
            bool hasValidId() const
            {
                return m_hasValidId;
            }

            void free() override
            {
                MemoryBlob::free();
                m_loaded = false;
                m_hasValidId = false;
                m_blobId.gds_quad_high = 0;
                m_blobId.gds_quad_low = 0;
            }
        };

    } // namespace Firebird
} // namespace cpp_dbc

#endif // USE_FIREBIRD

#endif // CPP_DBC_FIREBIRD_BLOB_HPP