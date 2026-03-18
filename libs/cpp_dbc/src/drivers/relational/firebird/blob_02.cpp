/*

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

 @file blob_02.cpp
 @brief Firebird database driver implementation - FirebirdBlob nothrow methods (factories, overrides, ensureLoaded, save)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    // ── Nothrow static factories ─────────────────────────────────────────────────
    // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

    cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> FirebirdBlob::create(std::nothrow_t,
                                                                                        std::weak_ptr<FirebirdDBConnection> conn) noexcept
    {
        auto obj = std::make_shared<FirebirdBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn));
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> FirebirdBlob::create(std::nothrow_t,
                                                                                        std::weak_ptr<FirebirdDBConnection> conn,
                                                                                        ISC_QUAD blobId) noexcept
    {
        auto obj = std::make_shared<FirebirdBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn), blobId);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> FirebirdBlob::create(std::nothrow_t,
                                                                                        std::weak_ptr<FirebirdDBConnection> conn,
                                                                                        const std::vector<uint8_t> &initialData) noexcept
    {
        auto obj = std::make_shared<FirebirdBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn), initialData);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    // ── Nothrow public methods ───────────────────────────────────────────────────

    cpp_dbc::expected<void, DBException> FirebirdBlob::copyFrom(std::nothrow_t, const FirebirdBlob &other) noexcept
    {
        if (this == &other)
        {
            return {};
        }
        auto r = MemoryBlob::copyFrom(std::nothrow, other);
        if (!r.has_value())
        {
            return r;
        }
        m_connection = other.m_connection;
        m_blobId = other.m_blobId;
        m_loaded = other.m_loaded;
        m_hasValidId = other.m_hasValidId;
        return {};
    }

    cpp_dbc::expected<void, DBException> FirebirdBlob::ensureLoaded(std::nothrow_t) const noexcept
    {
        if (m_loaded || !m_hasValidId)
        {
            return {};
        }

        // Acquire connection lock for the entire ISC blob lifecycle
        FIREBIRD_STMT_LOCK_OR_RETURN("K3M7N9P2Q5R8", "Blob connection closed");

        auto connResult = getConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        isc_db_handle *db = getDbHandle(std::nothrow);
        isc_tr_handle *tr = getTrHandle(std::nothrow);

        if (!db || !tr)
        {
            return cpp_dbc::unexpected(DBException("K3M7N9P2Q5R8", "Invalid database or transaction handle", system_utils::captureCallStack()));
        }

        ISC_STATUS_ARRAY status;
        isc_blob_handle blobHandle = 0;

        // Open the blob
        if (isc_open_blob2(status, db, tr, &blobHandle, &m_blobId, 0, nullptr))
        {
            return cpp_dbc::unexpected(DBException("K3M7N9P2Q5R8", "Failed to open BLOB for reading", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected(DBException("L4N8P0Q6R2S9", "Failed to read BLOB segment", system_utils::captureCallStack()));
            }

            m_data.insert(m_data.end(), buffer.begin(), buffer.begin() + actualLength);
        }

        // Close the blob
        if (isc_close_blob(status, &blobHandle))
        {
            return cpp_dbc::unexpected(DBException("M5P9Q1R7S3T0", "Failed to close BLOB after reading", system_utils::captureCallStack()));
        }

        m_loaded = true;
        return {};
    }

    cpp_dbc::expected<ISC_QUAD, DBException> FirebirdBlob::save(std::nothrow_t) noexcept
    {
        // Ensure BLOB data is loaded before saving
        auto loadResult = ensureLoaded(std::nothrow);
        if (!loadResult.has_value())
        {
            return cpp_dbc::unexpected(loadResult.error());
        }

        // Acquire connection lock for the entire ISC blob lifecycle
        FIREBIRD_STMT_LOCK_OR_RETURN("N6Q0R2S8T4U1", "Blob connection closed");

        auto connResult = getConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        isc_db_handle *db = getDbHandle(std::nothrow);
        isc_tr_handle *tr = getTrHandle(std::nothrow);

        if (!db || !tr)
        {
            return cpp_dbc::unexpected(DBException("N6Q0R2S8T4U1", "Invalid database or transaction handle", system_utils::captureCallStack()));
        }

        ISC_STATUS_ARRAY status;
        isc_blob_handle blobHandle = 0;

        // Create a new blob
        if (isc_create_blob2(status, db, tr, &blobHandle, &m_blobId, 0, nullptr))
        {
            return cpp_dbc::unexpected(DBException("N6Q0R2S8T4U1", "Failed to create BLOB for writing", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected(DBException("P7R1S3T9U5V2", "Failed to write BLOB segment", system_utils::captureCallStack()));
            }

            offset += chunkSize;
        }

        // Close the blob
        if (isc_close_blob(status, &blobHandle))
        {
            return cpp_dbc::unexpected(DBException("Q8S2T4U0V6W3", "Failed to close BLOB after writing", system_utils::captureCallStack()));
        }

        m_hasValidId = true;
        return m_blobId;
    }

    // ── Nothrow Blob overrides ───────────────────────────────────────────────────

    cpp_dbc::expected<size_t, DBException> FirebirdBlob::length(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::length(std::nothrow);
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> FirebirdBlob::getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::getBytes(std::nothrow, pos, length);
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> FirebirdBlob::getBinaryStream(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        // Use FirebirdInputStream which stores a COPY of the data,
        // not MemoryInputStream which stores a reference.
        // This ensures the data remains valid even after the blob is destroyed.
        auto streamResult = FirebirdInputStream::create(std::nothrow, m_data.data(), m_data.size());
        if (!streamResult.has_value())
        {
            return cpp_dbc::unexpected(streamResult.error());
        }
        return std::shared_ptr<InputStream>(streamResult.value());
    }

    cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> FirebirdBlob::setBinaryStream(std::nothrow_t, size_t pos) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBinaryStream(std::nothrow, pos);
    }

    cpp_dbc::expected<void, DBException> FirebirdBlob::setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes);
    }

    cpp_dbc::expected<void, DBException> FirebirdBlob::setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
    }

    cpp_dbc::expected<void, DBException> FirebirdBlob::truncate(std::nothrow_t, size_t len) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::truncate(std::nothrow, len);
    }

    cpp_dbc::expected<void, DBException> FirebirdBlob::free(std::nothrow_t) noexcept
    {
        auto r = MemoryBlob::free(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        m_loaded = false;
        m_hasValidId = false;
        m_blobId.gds_quad_high = 0;
        m_blobId.gds_quad_low = 0;
        return {};
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
