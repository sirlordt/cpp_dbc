#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"
#include "input_stream.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <ibase.h>
#include <memory>
#include <cstring>
#include <vector>

namespace cpp_dbc::Firebird
{
        // Forward declaration for weak_ptr usage
        class FirebirdDBConnection;

        /**
         * @brief Firebird implementation of Blob using smart pointers for memory safety
         *
         * This class uses std::weak_ptr to safely reference the FirebirdDBConnection,
         * preventing dangling pointer issues if the connection is closed while the
         * blob is still in use. All operations that require database access will
         * check if the connection is still valid before proceeding.
         */
        class FirebirdBlob : public MemoryBlob
        {
        private:
            /**
             * @brief Weak reference to the Firebird connection
             *
             * Using weak_ptr allows us to detect when the connection has been closed
             * and avoid use-after-free errors. The connection owns the database and
             * transaction handles, so we must ensure it's still valid before using them.
             */
            std::weak_ptr<FirebirdDBConnection> m_connection;

            mutable ISC_QUAD m_blobId;
            mutable bool m_loaded{false};
            bool m_hasValidId{false};

            /**
             * @brief Get a locked shared_ptr to the connection
             * @return shared_ptr to the connection
             * @throws DBException if the connection has been closed
             */
            cpp_dbc::expected<std::shared_ptr<FirebirdDBConnection>, DBException> getConnection(std::nothrow_t) const noexcept
            {
                auto conn = m_connection.lock();
                if (!conn)
                {
                    return cpp_dbc::unexpected(DBException("FAPY178ZS5UI", "Connection has been closed", system_utils::captureCallStack()));
                }
                return conn;
            }

            std::shared_ptr<FirebirdDBConnection> getConnection() const
            {
                auto r = getConnection(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            /**
             * @brief Get the database handle from the connection
             * @return Pointer to the database handle
             * @note This method is implemented after FirebirdConnection is defined
             */
            isc_db_handle *getDbHandle() const;

            /**
             * @brief Get the transaction handle from the connection
             * @return Pointer to the transaction handle
             * @note This method is implemented after FirebirdConnection is defined
             */
            isc_tr_handle *getTrHandle() const;

        public:
            /**
             * @brief Constructor for creating a new BLOB
             * @param connection Shared pointer to the Firebird connection
             */
            FirebirdBlob(std::shared_ptr<FirebirdDBConnection> connection)
                : m_connection(connection), m_loaded(true), m_hasValidId(false)
            {
                m_blobId.gds_quad_high = 0;
                m_blobId.gds_quad_low = 0;
            }

            /**
             * @brief Constructor for loading an existing BLOB by ID
             * @param connection Shared pointer to the Firebird connection
             * @param blobId The BLOB ID to load
             */
            FirebirdBlob(std::shared_ptr<FirebirdDBConnection> connection, ISC_QUAD blobId)
                : m_connection(connection), m_blobId(blobId), m_loaded(false), m_hasValidId(true) {}

            /**
             * @brief Constructor for creating a BLOB from existing data
             * @param connection Shared pointer to the Firebird connection
             * @param initialData The initial data for the BLOB
             */
            FirebirdBlob(std::shared_ptr<FirebirdDBConnection> connection, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), m_connection(connection), m_loaded(true), m_hasValidId(false)
            {
                m_blobId.gds_quad_high = 0;
                m_blobId.gds_quad_low = 0;
            }

            static cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> create(std::nothrow_t,
                std::shared_ptr<FirebirdDBConnection> connection) noexcept
            {
                return std::make_shared<FirebirdBlob>(connection);
            }

            static cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> create(std::nothrow_t,
                std::shared_ptr<FirebirdDBConnection> connection, ISC_QUAD blobId) noexcept
            {
                return std::make_shared<FirebirdBlob>(connection, blobId);
            }

            static cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> create(std::nothrow_t,
                std::shared_ptr<FirebirdDBConnection> connection, const std::vector<uint8_t> &initialData) noexcept
            {
                return std::make_shared<FirebirdBlob>(connection, initialData);
            }

            static std::shared_ptr<FirebirdBlob> create(std::shared_ptr<FirebirdDBConnection> connection)
            {
                auto r = create(std::nothrow, connection);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            static std::shared_ptr<FirebirdBlob> create(std::shared_ptr<FirebirdDBConnection> connection, ISC_QUAD blobId)
            {
                auto r = create(std::nothrow, connection, blobId);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            static std::shared_ptr<FirebirdBlob> create(std::shared_ptr<FirebirdDBConnection> connection,
                const std::vector<uint8_t> &initialData)
            {
                auto r = create(std::nothrow, connection, initialData);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const FirebirdBlob &other) noexcept
            {
                if (this == &other) { return {}; }
                auto r = MemoryBlob::copyFrom(std::nothrow, other);
                if (!r.has_value()) { return r; }
                m_connection = other.m_connection;
                m_blobId = other.m_blobId;
                m_loaded = other.m_loaded;
                m_hasValidId = other.m_hasValidId;
                return {};
            }

            void copyFrom(const FirebirdBlob &other)
            {
                auto r = copyFrom(std::nothrow, other);
                if (!r.has_value()) { throw r.error(); }
            }

            /**
             * @brief Load the BLOB data from the database if not already loaded
             *
             * This method safely accesses the connection through the weak_ptr,
             * ensuring the connection is still valid before attempting to read.
             */
            cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept
            {
                if (m_loaded || !m_hasValidId)
                {
                    return {};
                }

                // Get connection safely
                auto connResult = getConnection(std::nothrow);
                if (!connResult.has_value()) { return cpp_dbc::unexpected(connResult.error()); }
                isc_db_handle *db = getDbHandle();
                isc_tr_handle *tr = getTrHandle();

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

            void ensureLoaded() const
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
            }

            /**
             * @brief Get the BLOB ID
             * @return The BLOB ID
             */
            ISC_QUAD getBlobId() const
            {
                return m_blobId;
            }

            /**
             * @brief Check if the blob has a valid ID
             * @return true if the blob has been saved and has a valid ID
             */
            bool hasValidId() const
            {
                return m_hasValidId;
            }

            /**
             * @brief Check if the connection is still valid
             * @return true if the connection is still valid
             */
            bool isConnectionValid() const
            {
                return !m_connection.expired();
            }

            // ====================================================================
            // THROWING API - Exception-based (requires __cpp_exceptions)
            // ====================================================================

            #ifdef __cpp_exceptions

            size_t length() const override
            {
                auto r = length(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                auto r = getBytes(std::nothrow, pos, length);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                auto r = getBinaryStream(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
            {
                auto r = setBinaryStream(std::nothrow, pos);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
            {
                auto r = setBytes(std::nothrow, pos, bytes);
                if (!r.has_value()) { throw r.error(); }
            }

            void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
            {
                auto r = setBytes(std::nothrow, pos, bytes, length);
                if (!r.has_value()) { throw r.error(); }
            }

            void truncate(size_t len) override
            {
                auto r = truncate(std::nothrow, len);
                if (!r.has_value()) { throw r.error(); }
            }

            /**
             * @brief Save the BLOB data to the database and return the blob ID
             *
             * This method safely accesses the connection through the weak_ptr,
             * ensuring the connection is still valid before attempting to write.
             *
             * @return The BLOB ID that can be used to reference this BLOB
             * @throws DBException if the connection has been closed or if writing fails
             */
            ISC_QUAD save()
            {
                auto r = save(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            void free() override
            {
                auto r = free(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
            }

            #endif // __cpp_exceptions
            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

            cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::length(std::nothrow);
            }

            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::getBytes(std::nothrow, pos, length);
            }

            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                // Use FirebirdInputStream which stores a COPY of the data,
                // not MemoryInputStream which stores a reference.
                // This ensures the data remains valid even after the blob is destroyed.
                return std::shared_ptr<InputStream>(std::make_shared<FirebirdInputStream>(m_data.data(), m_data.size()));
            }

            cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBinaryStream(std::nothrow, pos);
            }

            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBytes(std::nothrow, pos, bytes);
            }

            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
            }

            cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::truncate(std::nothrow, len);
            }

            cpp_dbc::expected<ISC_QUAD, DBException> save(std::nothrow_t) noexcept
            {
                // Get connection safely
                auto connResult = getConnection(std::nothrow);
                if (!connResult.has_value()) { return cpp_dbc::unexpected(connResult.error()); }
                isc_db_handle *db = getDbHandle();
                isc_tr_handle *tr = getTrHandle();

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

            cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override
            {
                auto r = MemoryBlob::free(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                m_loaded = false;
                m_hasValidId = false;
                m_blobId.gds_quad_high = 0;
                m_blobId.gds_quad_low = 0;
                return {};
            }
        };

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
