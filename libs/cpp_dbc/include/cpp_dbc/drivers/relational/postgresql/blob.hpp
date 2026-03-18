#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"
#include "handles.hpp"

#if USE_POSTGRESQL
#include <memory>
#include <cstring>
#include <string>

#include "input_stream.hpp"

namespace cpp_dbc::PostgreSQL
{
    class PostgreSQLDBConnection;  // Forward declaration for weak_ptr and friend
    class PostgreSQLConnectionLock; // Forward declaration for friend

    /**
     * @brief PostgreSQL implementation of Blob with database-backed lazy loading
     *
     * Extends MemoryBlob with database-backed lazy loading via PostgreSQL large object API.
     * Synchronizes DB access through the parent connection's mutex via
     * weak_ptr<PostgreSQLDBConnection>, following the same pattern as
     * PreparedStatement and ResultSet.
     *
     * @see MemoryBlob, PostgreSQLDBResultSet, PostgreSQLDBConnection
     */
    class PostgreSQLBlob : public MemoryBlob
    {
        friend class PostgreSQLConnectionLock;

        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        using MemoryBlob::copyFrom; // Unhide base-class copyFrom overloads (cpp:S1242)

        // ── Member variables ──────────────────────────────────────────────────

        /**
         * @brief Weak reference to parent connection for mutex acquisition
         *
         * Used by PostgreSQLConnectionLock (RAII helper) to acquire the connection's
         * mutex. When expired, the blob operates in pure in-memory mode
         * (no DB I/O possible).
         */
        std::weak_ptr<PostgreSQLDBConnection> m_connection;

        mutable Oid m_lobOid{0};
        mutable bool m_loaded{false};
        mutable std::atomic<bool> m_closed{false};

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Private helpers ───────────────────────────────────────────────────

        /**
         * @brief Get PGconn* safely through the connection
         * @return Raw pointer to the PGconn handle or error if connection is closed
         *
         * Must be called under the connection lock (via POSTGRESQL_STMT_LOCK_OR_RETURN).
         */
        cpp_dbc::expected<PGconn *, DBException> getPGConnection(std::nothrow_t) const noexcept;

        /**
         * @brief Begin a transaction scope for large-object operations
         *
         * PostgreSQL requires all lo_* calls to occur within a transaction.
         * If the caller is already inside a transaction, a SAVEPOINT is used.
         * If idle, BEGIN is issued.
         */
        cpp_dbc::expected<void, DBException> beginLobScope(std::nothrow_t, PGconn *conn,
                                                            const char *savepointName,
                                                            bool &ownTransaction) const noexcept;

        /**
         * @brief Commit/release a transaction scope for large-object operations
         */
        cpp_dbc::expected<void, DBException> commitLobScope(std::nothrow_t, PGconn *conn,
                                                             const char *savepointName,
                                                             bool ownTransaction) const noexcept;

        /**
         * @brief Rollback a transaction scope (best-effort, errors silently ignored)
         */
        void rollbackLobScope(std::nothrow_t, PGconn *conn,
                              const char *savepointName,
                              bool ownTransaction) const noexcept;

    public:
        // ── Public nothrow constructors (guarded by PrivateCtorTag) ─────────────
        // No try/catch: all operations are death-sentence (std::bad_alloc from
        // vector copies) or trivially noexcept (weak_ptr copy, Oid/bool assign).

        /** @brief Construct an empty BLOB for in-memory use */
        PostgreSQLBlob(PrivateCtorTag, std::nothrow_t,
                       std::weak_ptr<PostgreSQLDBConnection> conn) noexcept
            : m_loaded(true)
        {
            m_connection = std::move(conn);
        }

        /** @brief Construct a lazy-loading BLOB by OID */
        PostgreSQLBlob(PrivateCtorTag, std::nothrow_t,
                       std::weak_ptr<PostgreSQLDBConnection> conn, Oid oid) noexcept
            : m_lobOid(oid)
        {
            m_connection = std::move(conn);
        }

        /** @brief Construct a BLOB from existing binary data */
        PostgreSQLBlob(PrivateCtorTag, std::nothrow_t,
                       std::weak_ptr<PostgreSQLDBConnection> conn,
                       const std::vector<uint8_t> &initialData) noexcept
            : MemoryBlob(initialData), m_loaded(true)
        {
            m_connection = std::move(conn);
        }

        ~PostgreSQLBlob() override = default;

        PostgreSQLBlob(const PostgreSQLBlob &) = delete;
        PostgreSQLBlob &operator=(const PostgreSQLBlob &) = delete;
        PostgreSQLBlob(PostgreSQLBlob &&) = delete;
        PostgreSQLBlob &operator=(PostgreSQLBlob &&) = delete;

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        // ── Throwing static factories (delegate to nothrow) ──────────────────

        static std::shared_ptr<PostgreSQLBlob> create(std::weak_ptr<PostgreSQLDBConnection> conn);
        static std::shared_ptr<PostgreSQLBlob> create(std::weak_ptr<PostgreSQLDBConnection> conn, Oid oid);
        static std::shared_ptr<PostgreSQLBlob> create(std::weak_ptr<PostgreSQLDBConnection> conn,
                                                       const std::vector<uint8_t> &initialData);

        // ── Throwing public methods ──────────────────────────────────────────

        void copyFrom(const PostgreSQLBlob &other);
        void ensureLoaded() const;
        size_t length() const override;
        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override;
        std::shared_ptr<InputStream> getBinaryStream() const override;
        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override;
        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override;
        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override;
        void truncate(size_t len) override;
        Oid save();
        void free() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API - Exception-free (always available)
        // ====================================================================

        // ── Nothrow static factories ─────────────────────────────────────────
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t,
                                                                                       std::weak_ptr<PostgreSQLDBConnection> conn) noexcept;

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t,
                                                                                       std::weak_ptr<PostgreSQLDBConnection> conn, Oid oid) noexcept;

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t,
                                                                                       std::weak_ptr<PostgreSQLDBConnection> conn,
                                                                                       const std::vector<uint8_t> &initialData) noexcept;

        // ── Nothrow public methods ───────────────────────────────────────────

        Oid getOid() const noexcept
        {
            return m_lobOid;
        }

        bool isConnectionValid() const noexcept override
        {
            return !m_connection.expired();
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const PostgreSQLBlob &other) noexcept;
        cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept;
        cpp_dbc::expected<Oid, DBException> save(std::nothrow_t) noexcept;

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

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
