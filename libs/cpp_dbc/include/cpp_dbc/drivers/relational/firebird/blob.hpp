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
    class FirebirdDBConnection;  // Forward declaration for weak_ptr and friend
    class FirebirdConnectionLock; // Forward declaration for friend

    /**
     * @brief Firebird implementation of Blob with database-backed lazy loading
     *
     * Extends MemoryBlob with database-backed lazy loading via Firebird ISC API.
     * Synchronizes DB access through the parent connection's mutex via
     * weak_ptr<FirebirdDBConnection>, following the same pattern as
     * PreparedStatement and ResultSet.
     *
     * @see MemoryBlob, FirebirdDBResultSet, FirebirdDBConnection
     */
    class FirebirdBlob : public MemoryBlob
    {
        friend class FirebirdConnectionLock;

        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // ── Member variables ──────────────────────────────────────────────────

        /**
         * @brief Weak reference to parent connection for mutex acquisition
         *
         * Used by FirebirdConnectionLock (RAII helper) to acquire the connection's
         * mutex. When expired, the blob operates in pure in-memory mode
         * (no DB I/O possible).
         */
        std::weak_ptr<FirebirdDBConnection> m_connection;

        mutable ISC_QUAD m_blobId{};
        mutable bool m_loaded{false};
        bool m_hasValidId{false};
        mutable std::atomic<bool> m_closed{false};

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Private helpers ───────────────────────────────────────────────────

        /**
         * @brief Get a locked shared_ptr to the connection
         * @return shared_ptr to the connection or error if closed
         *
         * Must be called under the connection lock (via FIREBIRD_STMT_LOCK_OR_RETURN).
         */
        cpp_dbc::expected<std::shared_ptr<FirebirdDBConnection>, DBException> getConnection(std::nothrow_t) const noexcept;

        /**
         * @brief Get the database handle from the connection
         * @return Pointer to the database handle
         */
        isc_db_handle *getDbHandle(std::nothrow_t) const noexcept;

        /**
         * @brief Get the transaction handle from the connection
         * @return Pointer to the transaction handle
         */
        isc_tr_handle *getTrHandle(std::nothrow_t) const noexcept;

    public:
        // ── Public nothrow constructors (guarded by PrivateCtorTag) ─────────────
        // No try/catch: all operations are death-sentence (std::bad_alloc from
        // vector copies) or trivially noexcept (weak_ptr copy, bool/ISC_QUAD assign).

        /** @brief Construct an empty BLOB for in-memory use */
        FirebirdBlob(PrivateCtorTag, std::nothrow_t,
                     std::weak_ptr<FirebirdDBConnection> conn) noexcept
            : m_loaded(true)
        {
            m_connection = std::move(conn);
        }

        /** @brief Construct a lazy-loading BLOB by ID */
        FirebirdBlob(PrivateCtorTag, std::nothrow_t,
                     std::weak_ptr<FirebirdDBConnection> conn,
                     ISC_QUAD blobId) noexcept
            : m_blobId(blobId), m_hasValidId(true)
        {
            m_connection = std::move(conn);
        }

        /** @brief Construct a BLOB from existing binary data */
        FirebirdBlob(PrivateCtorTag, std::nothrow_t,
                     std::weak_ptr<FirebirdDBConnection> conn,
                     const std::vector<uint8_t> &initialData) noexcept
            : MemoryBlob(initialData), m_loaded(true)
        {
            m_connection = std::move(conn);
        }

        ~FirebirdBlob() override = default;

        FirebirdBlob(const FirebirdBlob &) = delete;
        FirebirdBlob &operator=(const FirebirdBlob &) = delete;

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        // ── Throwing static factories (delegate to nothrow) ──────────────────

        static std::shared_ptr<FirebirdBlob> create(std::weak_ptr<FirebirdDBConnection> conn);
        static std::shared_ptr<FirebirdBlob> create(std::weak_ptr<FirebirdDBConnection> conn, ISC_QUAD blobId);
        static std::shared_ptr<FirebirdBlob> create(std::weak_ptr<FirebirdDBConnection> conn,
                                                     const std::vector<uint8_t> &initialData);

        // ── Throwing public methods ──────────────────────────────────────────

        void copyFrom(const FirebirdBlob &other);
        void ensureLoaded() const;
        size_t length() const override;
        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override;
        std::shared_ptr<InputStream> getBinaryStream() const override;
        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override;
        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override;
        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override;
        void truncate(size_t len) override;
        ISC_QUAD save();
        void free() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API - Exception-free (always available)
        // ====================================================================

        // ── Nothrow static factories ─────────────────────────────────────────
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

        static cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> create(std::nothrow_t,
                                                                                     std::weak_ptr<FirebirdDBConnection> conn) noexcept;

        static cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> create(std::nothrow_t,
                                                                                     std::weak_ptr<FirebirdDBConnection> conn,
                                                                                     ISC_QUAD blobId) noexcept;

        static cpp_dbc::expected<std::shared_ptr<FirebirdBlob>, DBException> create(std::nothrow_t,
                                                                                     std::weak_ptr<FirebirdDBConnection> conn,
                                                                                     const std::vector<uint8_t> &initialData) noexcept;

        // ── Nothrow public methods ───────────────────────────────────────────

        ISC_QUAD getBlobId() const noexcept
        {
            return m_blobId;
        }

        bool hasValidId() const noexcept
        {
            return m_hasValidId;
        }

        bool isConnectionValid() const noexcept override
        {
            return !m_connection.expired();
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const FirebirdBlob &other) noexcept;
        cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept;
        cpp_dbc::expected<ISC_QUAD, DBException> save(std::nothrow_t) noexcept;

        // ── Nothrow Blob overrides ───────────────────────────────────────────

        cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override;
        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override;
        cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override;
        cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
